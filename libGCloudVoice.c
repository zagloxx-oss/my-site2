#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "AWANGARD", __VA_ARGS__)

// ==================== ОФФСЕТЫ TslGame 2609.1.3.1 ====================
#define OFF_UWorld              0x128271E8
#define OFF_GameState           0x88
#define OFF_PlayerArray         0x418
#define OFF_PlayerController    0x38
#define OFF_AcknowledgedPawn    0x4A8
#define OFF_RootComponent       0x3E8
#define OFF_ComponentLocation   0x330
#define OFF_PlayerCameraManager 0x4D0
#define OFF_CameraCacheFOV      0x10C0
#define OFF_WeaponProcessor     0x958
#define OFF_EquippedWeapons     0x210
#define OFF_WeaponTrajectory    0x1220
#define OFF_RecoilValue         0x11B0
#define OFF_VerticalRecovery    0x11B8
#define OFF_DeltaTimeSeconds    0x8C0

static int mem_fd = -1;
static unsigned long ue4_base = 0;

static unsigned long read_ptr(unsigned long addr) {
    unsigned long v = 0;
    if (lseek(mem_fd, addr, SEEK_SET) < 0) return 0;
    if (read(mem_fd, &v, 8) != 8) return 0;
    return v;
}

static void write_float(unsigned long addr, float val) {
    lseek(mem_fd, addr, SEEK_SET);
    write(mem_fd, &val, 4);
}

static unsigned long find_base(const char *name) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512];
    unsigned long base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) {
            sscanf(line, "%lx", &base);
            break;
        }
    }
    fclose(f);
    return base;
}

static unsigned long get_uworld() {
    if (!ue4_base) return 0;
    return read_ptr(ue4_base + OFF_UWorld);
}

static unsigned long get_local_pawn() {
    unsigned long uworld = get_uworld();
    if (!uworld) return 0;
    unsigned long gs = read_ptr(uworld + OFF_GameState);
    if (!gs) return 0;
    unsigned long pa = read_ptr(gs + OFF_PlayerArray);
    if (!pa) return 0;
    unsigned long pc = read_ptr(pa);
    if (!pc) return 0;
    return read_ptr(pc + OFF_AcknowledgedPawn);
}

// ==================== ФУНКЦИИ ====================
void cheat_no_recoil(int enable) {
    LOG("no_recoil(%d)", enable);
    unsigned long pawn = get_local_pawn();
    if (!pawn) { LOG("pawn NULL"); return; }
    unsigned long wp = read_ptr(pawn + OFF_WeaponProcessor);
    if (!wp) return;
    unsigned long weapons = read_ptr(wp + OFF_EquippedWeapons);
    if (!weapons) return;
    unsigned long cur = read_ptr(weapons);
    if (!cur) return;
    unsigned long traj = read_ptr(cur + OFF_WeaponTrajectory);
    if (!traj) return;
    if (enable) {
        write_float(traj + OFF_RecoilValue, 0.0f);
        write_float(traj + OFF_RecoilValue + 4, 0.0f);
        write_float(traj + OFF_VerticalRecovery, 0.0f);
        LOG("no_recoil ON");
    } else {
        write_float(traj + OFF_RecoilValue, 1.0f);
        write_float(traj + OFF_VerticalRecovery, 1.0f);
        LOG("no_recoil OFF");
    }
}

void cheat_set_fov(float fov) {
    unsigned long pawn = get_local_pawn();
    if (!pawn) return;
    unsigned long pc = read_ptr(pawn + OFF_PlayerController);
    if (!pc) return;
    unsigned long pcm = read_ptr(pc + OFF_PlayerCameraManager);
    if (!pcm) return;
    write_float(pcm + OFF_CameraCacheFOV, fov);
    LOG("FOV = %.1f", fov);
}

void cheat_120fps(int enable) {
    unsigned long uworld = get_uworld();
    if (!uworld) return;
    write_float(uworld + OFF_DeltaTimeSeconds, enable ? (1.0f/120.0f) : (1.0f/60.0f));
    LOG("120fps: %d", enable);
}

void cheat_magic_bullets(int enable) {
    LOG("magic_bullets: %d (stub)", enable);
}

__attribute__((constructor))
void lib_init(void) {
    LOG("=== AWANGARD CHEAT loaded ===");
    LOG("pid = %d", getpid());
    mem_fd = open("/proc/self/mem", O_RDWR);
    if (mem_fd < 0) { LOG("ERROR /proc/self/mem"); return; }
    ue4_base = find_base("libUE4.so");
    if (!ue4_base) { LOG("ERROR libUE4.so not found"); return; }
    LOG("libUE4.so base = 0x%lx", ue4_base);
    LOG("UWorld = 0x%lx", get_uworld());
    LOG("LocalPawn = 0x%lx", get_local_pawn());
    LOG("=== INIT DONE ===");
}
