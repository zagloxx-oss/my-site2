#include <jni.h>
#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <mutex>
#include <dlfcn.h>
#include <unistd.h>
#include <link.h>
#include <elf.h>

#define LOG_TAG "UE4Cheat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static std::ofstream g_logFile;
static std::mutex g_logMutex;

// ========== ЛОГИРОВАНИЕ ==========
void log_msg(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    LOGI("%s", msg.c_str());
    if (g_logFile.is_open()) {
        g_logFile << msg << std::endl;
        g_logFile.flush();
    }
}

// ========== ПОИСК БАЗЫ МОДУЛЯ ЧЕРЕЗ /proc/self/maps ==========
uintptr_t find_module_base(const char* module_name) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;

    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, module_name)) {
            char* dash = strchr(line, '-');
            if (dash) {
                *dash = '\0';
                base = strtoul(line, nullptr, 16);
                break;
            }
        }
    }
    fclose(fp);
    return base;
}

// ========== ПАРСИНГ ELF: ПОИСК ЭКСПОРТИРОВАННЫХ СИМВОЛОВ ==========
// Возвращает адрес символа по имени в модуле
uintptr_t resolve_symbol_manual(uintptr_t base, const char* symbol_name) {
    if (!base) return 0;

    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)base;
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0) {
        log_msg("[-] Не ELF64 файл");
        return 0;
    }

    // Ищем секцию .dynsym и .dynstr
    Elf64_Shdr* shdr = (Elf64_Shdr*)(base + ehdr->e_shoff);
    const char* shstrtab = (const char*)(base + shdr[ehdr->e_shstrndx].sh_offset);

    Elf64_Sym* dynsym = nullptr;
    const char* dynstr = nullptr;
    size_t dynsym_count = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char* name = shstrtab + shdr[i].sh_name;
        if (strcmp(name, ".dynsym") == 0) {
            dynsym = (Elf64_Sym*)(base + shdr[i].sh_offset);
            dynsym_count = shdr[i].sh_size / sizeof(Elf64_Sym);
        } else if (strcmp(name, ".dynstr") == 0) {
            dynstr = (const char*)(base + shdr[i].sh_offset);
        }
    }

    if (!dynsym || !dynstr) {
        log_msg("[-] .dynsym или .dynstr не найдены");
        return 0;
    }

    for (size_t i = 0; i < dynsym_count; i++) {
        if (dynsym[i].st_name == 0) continue;
        const char* name = dynstr + dynsym[i].st_name;
        if (strcmp(name, symbol_name) == 0) {
            return base + dynsym[i].st_value;
        }
    }
    return 0;
}

// ========== ДАМП ВСЕХ ЭКСПОРТОВ ==========
void dump_all_symbols(uintptr_t base) {
    if (!base) return;

    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)base;
    Elf64_Shdr* shdr = (Elf64_Shdr*)(base + ehdr->e_shoff);
    const char* shstrtab = (const char*)(base + shdr[ehdr->e_shstrndx].sh_offset);

    Elf64_Sym* dynsym = nullptr;
    const char* dynstr = nullptr;
    size_t dynsym_count = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char* name = shstrtab + shdr[i].sh_name;
        if (strcmp(name, ".dynsym") == 0) {
            dynsym = (Elf64_Sym*)(base + shdr[i].sh_offset);
            dynsym_count = shdr[i].sh_size / sizeof(Elf64_Sym);
        } else if (strcmp(name, ".dynstr") == 0) {
            dynstr = (const char*)(base + shdr[i].sh_offset);
        }
    }

    if (!dynsym || !dynstr) return;

    int count = 0;
    for (size_t i = 0; i < dynsym_count; i++) {
        if (dynsym[i].st_name == 0) continue;
        if (ELF64_ST_TYPE(dynsym[i].st_info) != STT_FUNC) continue;

        const char* name = dynstr + dynsym[i].st_name;
        char buf[512];
        snprintf(buf, sizeof(buf), "[SYM] %s @ +0x%lx", name, (unsigned long)dynsym[i].st_value);
        log_msg(buf);

        count++;
        if (count > 500) {
            log_msg("[!] ... обрезано, слишком много символов");
            break;
        }
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "[+] Всего экспортированных функций: %d", count);
    log_msg(buf);
}

// ========== ИНИЦИАЛИЗАЦИЯ ==========
__attribute__((constructor))
void on_library_load() {
    g_logFile.open("/sdcard/UE4Cheat.log", std::ios::out | std::ios::trunc);
    if (!g_logFile.is_open()) {
        g_logFile.open("/data/local/tmp/UE4Cheat.log", std::ios::out | std::ios::trunc);
    }
    log_msg("=== UE4 Cheat Library v1.0 ===");

    // Ждём загрузки libUE4.so
    uintptr_t base = 0;
    int attempts = 0;
    while (attempts < 30) {
        base = find_module_base("libUE4.so");
        if (base) break;
        sleep(1);
        attempts++;
    }

    if (!base) {
        log_msg("[-] libUE4.so не найден за 30 секунд");
        return;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "[+] libUE4.so base: 0x%lx", (unsigned long)base);
    log_msg(buf);

    // Ищем ключевые символы
    const char* symbols_to_find[] = {
        "_ZN7UObject11ProcessEventEP9UFunctionPv",
        "_ZN7UObject7GetNameEv",
        "_ZN7UObject12GetFullNameEv",
        "_ZN8AActor4TickEf",
        "_ZN7UWorld9SpawnActorE",
        "_ZNK4FName8ToStringEv",
        "_ZN23UCharacterMovementComponent16SetMaxWalkSpeedEf",
    };

    for (const char* sym : symbols_to_find) {
        uintptr_t addr = resolve_symbol_manual(base, sym);
        if (addr) {
            snprintf(buf, sizeof(buf), "[+] %s -> +0x%lx", sym, (unsigned long)(addr - base));
        } else {
            snprintf(buf, sizeof(buf), "[-] Не найден: %s", sym);
        }
        log_msg(buf);
    }

    // Дамп всех символов
    log_msg("=== Дамп всех экспортированных символов ===");
    dump_all_symbols(base);

    log_msg("=== Готово. Смотри /sdcard/UE4Cheat.log ===");
}
