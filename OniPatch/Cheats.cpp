#include "pch.h"
#include "Cheats.h"

namespace
{
    // Player object and the fields used by the cheats.
    const DWORD PLAYER = 0x033F9B20;
    const DWORD PLAYER_MAX_HP = PLAYER + 0x21E4;   // 0x033FBD04, dword (1280 at the start)
    const DWORD PLAYER_HP = PLAYER + 0x21E8;       // 0x033FBD08, dword
    const DWORD RED_SOULS = PLAYER + 0x443C;       // 0x033FDF5C, dword
    const DWORD PURPLE_SOULS = PLAYER + 0x444C;    // 0x033FDF6C, byte

    // Chain issen: a landed issen opens a chain window only while chains remain (0x6BA930).
    const DWORD CHAINS_LEFT = PLAYER + 0x44DB;     // byte
    const BYTE CHAINS_KEPT = 5;

    // Inventory: 13 items of 8 bytes, one count byte per character (Samanosuke, Jacques, Michelle, Heihachi).
    const DWORD ITEM_COUNTS = 0x0340D6FC;
    const int ITEM_SLOTS = 13;
    const int ITEM_STRIDE = 8;
    const int CHARACTERS = 4;
    const DWORD ARROWS[] = { 0x0340D3CC, 0x0340D3D4, 0x0340D3DC, 0x0340D3E4, 0x0340D3EC }; // dwords
    const DWORD ECOSPIRIT = 0x0340D7F4;            // dword
    const DWORD GRENADES = 0x0340D49E;             // word

    // Code patched with a jump to the stubs below, with the original bytes checked first.
    const DWORD DAMAGE_SITE = 0x005DF10E;          // sub [esi+21E8],eax
    const BYTE DAMAGE_BYTES[] = { 0x29, 0x86, 0xE8, 0x21, 0x00, 0x00 };
    const DWORD ONI_POWER_SITE = 0x005466B2;       // add word ptr [ecx+4442],si
    const BYTE ONI_POWER_BYTES[] = { 0x66, 0x01, 0xB1, 0x42, 0x44, 0x00, 0x00 };
    // Issen. Attacks call the issen check 0x6BB910 (ecx = character), whose target search (0x6BA5D0)
    // skips the enemy's own timing check while bit 0x40000 of +0x22F4 is set. The game clears its
    // flags during the update, so the bit is set at the check's entry:
    // push ecx / push esi / mov esi,ecx / test dword ptr [esi+4444],40000.
    const DWORD ISSEN_CHECK_SITE = 0x006BB910;
    const BYTE ISSEN_CHECK_BYTES[] = { 0x51, 0x56, 0x8B, 0xF1, 0xF7, 0x86, 0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00 };

    // Guns. With a gun the issen check bails out early (bit 0x40000 of +0x4444), and even when it
    // passes, the shot overrides the queued issen action. Instead the shot's action number is
    // replaced: mov byte ptr [esi+2238],97 (normal shot) becomes 0x99 (gun critical).
    const DWORD GUN_SHOT_SITE = 0x006BB221;
    const BYTE GUN_SHOT_BYTES[] = { 0xC6, 0x86, 0x38, 0x22, 0x00, 0x00, 0x97 };

    // Movement. Every character's position is updated as old position + rotated animation
    // movement: movaps xmm1,xmm0 / addps xmm1,[esp+30], then stored to [eax] (or [edx] on the
    // second path), which points to the character's position (+0x90). Z is the height.
    const DWORD MOVE_SITE = 0x0053F5A7;
    const DWORD MOVE_SLOPE_SITE = 0x0053F602;
    const BYTE MOVE_BYTES[] = { 0x0F, 0x28, 0xC8, 0x0F, 0x58, 0x4C, 0x24, 0x30 };

    // In the chain window (0x6BCBB0) the next issen starts when the attack button is down:
    // test dword ptr [esp+8],2000 / je 6BCC8C.
    const DWORD CHAIN_BUTTON_SITE = 0x006BCBCC;
    const BYTE CHAIN_BUTTON_BYTES[] = { 0xF7, 0x44, 0x24, 0x08, 0x00, 0x20, 0x00, 0x00, 0x0F, 0x84, 0xB2, 0x00, 0x00, 0x00 };
    const DWORD CHAIN_PRESSED = 0x006BCBDA;
    const DWORD CHAIN_NOT_PRESSED = 0x006BCC8C;

    const int KEYS[CHEAT_COUNT] = { VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7 };
    // Pressed while LT is held. LT has no game action, so the combos never reach the game.
    const WORD COMBO_BUTTONS[CHEAT_COUNT] = {
        XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT,
        XINPUT_GAMEPAD_Y, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X
    };
    const WORD COMBO_MASK = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_DPAD_DOWN |
        XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_Y | XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_X;

    DWORD ReadValue(DWORD address, int size)
    {
        switch (size) {
        case 1: return *reinterpret_cast<volatile BYTE *>(address);
        case 2: return *reinterpret_cast<volatile WORD *>(address);
        default: return *reinterpret_cast<volatile DWORD *>(address);
        }
    }

    void WriteValue(DWORD address, int size, DWORD value)
    {
        switch (size) {
        case 1: *reinterpret_cast<volatile BYTE *>(address) = static_cast<BYTE>(value); break;
        case 2: *reinterpret_cast<volatile WORD *>(address) = static_cast<WORD>(value); break;
        default: *reinterpret_cast<volatile DWORD *>(address) = value; break;
        }
    }

    bool IsGameWindowActive()
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &pid);
        return pid == GetCurrentProcessId();
    }

    // Writes "jmp target" at site, padded with nops, if site still holds the expected bytes.
    bool PatchJump(DWORD site, const BYTE *expected, size_t length, void *target)
    {
        DWORD oldProtect;
        BYTE *code = reinterpret_cast<BYTE *>(site);

        if (memcmp(code, expected, length) != 0) {
            std::cerr << "Unexpected code at 0x" << std::hex << site << std::dec << ", cheat not installed" << std::endl;
            return false;
        }
        if (!VirtualProtect(code, length, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        code[0] = 0xE9;
        *reinterpret_cast<DWORD *>(code + 1) = reinterpret_cast<DWORD>(target) - (site + 5);
        for (size_t i = 5; i < length; i++)
            code[i] = 0x90;

        VirtualProtect(code, length, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), code, length);
        return true;
    }
}

// Read by the stubs, written by Cheats::Toggle.
extern "C" volatile BYTE g_cheatInvincible = 0;
extern "C" volatile BYTE g_cheatOneHitKill = 0;
extern "C" volatile BYTE g_cheatInfiniteOni = 0;
extern "C" volatile BYTE g_cheatAlwaysIssen = 0;
extern "C" volatile BYTE g_cheatAutoChain = 0;
extern "C" volatile BYTE g_cheatMoveSpeed = 0;
// x, y scaled; z (height) and w kept. Aligned for mulps.
__declspec(align(16)) static float moveFactor[4] = { 2.0f, 2.0f, 1.0f, 1.0f };
static DWORD moveReturn = MOVE_SITE + sizeof(MOVE_BYTES);
static DWORD moveSlopeReturn = MOVE_SLOPE_SITE + sizeof(MOVE_BYTES);
static DWORD damageReturn = DAMAGE_SITE + sizeof(DAMAGE_BYTES);
static DWORD issenCheckReturn = ISSEN_CHECK_SITE + sizeof(ISSEN_CHECK_BYTES);
static DWORD gunShotReturn = GUN_SHOT_SITE + sizeof(GUN_SHOT_BYTES);
static DWORD oniPowerReturn = ONI_POWER_SITE + sizeof(ONI_POWER_BYTES);
static DWORD chainPressed = CHAIN_PRESSED;
static DWORD chainNotPressed = CHAIN_NOT_PRESSED;

#ifdef _M_IX86
// Replaces "sub [esi+21E8],eax": esi is the character being hit, eax the damage, ebp the "dead" threshold.
static __declspec(naked) void DamageStub()
{
    __asm {
        cmp esi, 0x033F9B20             // PLAYER (inline asm reads C constants as memory)
        jne enemy
        cmp g_cheatInvincible, 0
        je original
        xor eax, eax                    // no damage to the player
        jmp original
    enemy:
        cmp g_cheatOneHitKill, 0
        je original
        mov eax, [esi + 0x21E8]         // damage = hp - threshold + 1, so hp ends below the threshold
        sub eax, ebp
        inc eax
    original:
        sub dword ptr [esi + 0x21E8], eax
        jmp damageReturn
    }
}

// Replaces "add word ptr [ecx+4442],si": si is the oni power change.
static __declspec(naked) void OniPowerStub()
{
    __asm {
        cmp g_cheatInfiniteOni, 0
        je original
        test si, si
        js done                         // skip only the decreases
    original:
        add word ptr [ecx + 0x4442], si
    done:
        jmp oniPowerReturn
    }
}

// Replaces the first instructions of the issen check: for the player, force the target check
// so any nearby enemy can be cut. The test comes last because the next instruction branches on it.
static __declspec(naked) void IssenCheckStub()
{
    __asm {
        cmp g_cheatAlwaysIssen, 0
        je original
        cmp ecx, 0x033F9B20             // PLAYER
        jne original
        or dword ptr [ecx + 0x22F4], 0x40000
    original:
        push ecx
        push esi
        mov esi, ecx
        test dword ptr [esi + 0x4444], 0x40000
        jmp issenCheckReturn
    }
}

// Replaces "mov byte ptr [esi+2238],97": the player's shots become gun criticals.
static __declspec(naked) void GunShotStub()
{
    __asm {
        cmp g_cheatAlwaysIssen, 0
        je normal
        cmp esi, 0x033F9B20             // PLAYER
        jne normal
        mov byte ptr [esi + 0x2238], 0x99
        jmp gunShotReturn
    normal:
        mov byte ptr [esi + 0x2238], 0x97
        jmp gunShotReturn
    }
}

// Replaces "movaps xmm1,xmm0 / addps xmm1,[esp+30]" before the position store at [eax]:
// scale the player's movement. The next instruction branches on "test edi,edi", redone last.
static __declspec(naked) void MoveStub()
{
    __asm {
        movaps xmm1, xmm0
        cmp g_cheatMoveSpeed, 0
        je addMove
        cmp eax, 0x033F9BB0             // PLAYER + 0x90 (position)
        jne addMove
        mulps xmm1, moveFactor
    addMove:
        addps xmm1, [esp + 0x30]
        test edi, edi
        jmp moveReturn
    }
}

// Same on the second path, where the position pointer is in edx.
static __declspec(naked) void MoveSlopeStub()
{
    __asm {
        movaps xmm1, xmm0
        cmp g_cheatMoveSpeed, 0
        je addMove
        cmp edx, 0x033F9BB0             // PLAYER + 0x90 (position)
        jne addMove
        mulps xmm1, moveFactor
    addMove:
        addps xmm1, [esp + 0x30]
        jmp moveSlopeReturn
    }
}

// Replaces "test dword ptr [esp+8],2000 / je 6BCC8C" in the chain window:
// with auto chain on, the attack button counts as pressed.
static __declspec(naked) void ChainButtonStub()
{
    __asm {
        cmp g_cheatAutoChain, 0
        jne pressed
        test dword ptr [esp + 8], 0x2000
        je notPressed
    pressed:
        jmp chainPressed
    notPressed:
        jmp chainNotPressed
    }
}
#endif

Cheats &Cheats::Get()
{
    static Cheats instance;
    return instance;
}

Cheats::Cheats()
    : enabled(), keyDown(), lastPadButtons(0), version(0)
{
    soulWatches.push_back({ RED_SOULS, 4, 0, false });
    soulWatches.push_back({ PURPLE_SOULS, 1, 0, false });

    for (int slot = 0; slot < ITEM_SLOTS; slot++)
        for (int character = 0; character < CHARACTERS; character++)
            itemWatches.push_back({ ITEM_COUNTS + slot * ITEM_STRIDE + character, 1, 0, false });
    for (DWORD arrows : ARROWS)
        itemWatches.push_back({ arrows, 4, 0, false });
    itemWatches.push_back({ ECOSPIRIT, 4, 0, false });
    itemWatches.push_back({ GRENADES, 2, 0, false });
}

void Cheats::InstallCodePatches()
{
#ifdef _M_IX86
    TraceFunc();
    PatchJump(DAMAGE_SITE, DAMAGE_BYTES, sizeof(DAMAGE_BYTES), DamageStub);
    PatchJump(ONI_POWER_SITE, ONI_POWER_BYTES, sizeof(ONI_POWER_BYTES), OniPowerStub);
    PatchJump(ISSEN_CHECK_SITE, ISSEN_CHECK_BYTES, sizeof(ISSEN_CHECK_BYTES), IssenCheckStub);
    PatchJump(GUN_SHOT_SITE, GUN_SHOT_BYTES, sizeof(GUN_SHOT_BYTES), GunShotStub);
    PatchJump(MOVE_SITE, MOVE_BYTES, sizeof(MOVE_BYTES), MoveStub);
    PatchJump(MOVE_SLOPE_SITE, MOVE_BYTES, sizeof(MOVE_BYTES), MoveSlopeStub);
    PatchJump(CHAIN_BUTTON_SITE, CHAIN_BUTTON_BYTES, sizeof(CHAIN_BUTTON_BYTES), ChainButtonStub);
#endif
}

bool Cheats::IsEnabled(Cheat cheat) const
{
    return enabled[static_cast<int>(cheat)];
}

std::wstring Cheats::Label(Cheat cheat)
{
    // Japanese labels as code points: the sources are compiled with the Shift-JIS code page,
    // where \u escapes in literals are not reliable.
    static const wchar_t INVINCIBLE[] = { 0x7121, 0x6575, 0 };                                          // muteki
    static const wchar_t INFINITE_ONI[] = { 0x9B3C, 0x529B, 0x30FB, 0x9B42, L' ', 0x7121, 0x9650, 0 };  // oni-ryoku / tamashii mugen
    static const wchar_t INFINITE_ITEMS[] = { 0x30A2, 0x30A4, 0x30C6, 0x30E0, L' ', 0x7121, 0x9650, 0 }; // aitemu mugen
    static const wchar_t ONE_HIT_KILL[] = { 0x4E00, 0x6483, 0x5FC5, 0x6BBA, 0 };                        // ichigeki hissatsu
    static const wchar_t ALWAYS_ISSEN[] = { 0x5E38, 0x6642, 0x4E00, 0x9583, 0 };                        // joji issen
    static const wchar_t AUTO_CHAIN[] = { 0x81EA, 0x52D5, 0x9023, 0x9396, 0 };                          // jido rensa
    static const wchar_t MOVE_SPEED[] = { 0x79FB, 0x52D5, 0x901F, 0x5EA6, L' ', 0x00D7, L'2', 0 };     // ido sokudo x2

    switch (cheat) {
    case Cheat::Invincible: return std::wstring(L"[F1] ") + INVINCIBLE;
    case Cheat::InfiniteOni: return std::wstring(L"[F2] ") + INFINITE_ONI;
    case Cheat::InfiniteItems: return std::wstring(L"[F3] ") + INFINITE_ITEMS;
    case Cheat::OneHitKill: return std::wstring(L"[F4] ") + ONE_HIT_KILL;
    case Cheat::AlwaysIssen: return std::wstring(L"[F5] ") + ALWAYS_ISSEN;
    case Cheat::AutoChain: return std::wstring(L"[F6] ") + AUTO_CHAIN;
    case Cheat::MoveSpeed: return std::wstring(L"[F7] ") + MOVE_SPEED;
    default: return L"";
    }
}

void Cheats::Toggle(Cheat cheat)
{
    int i = static_cast<int>(cheat);

    enabled[i] = !enabled[i];
    version++;

    // Start watching from the current values.
    if (cheat == Cheat::InfiniteOni)
        for (Watch &w : soulWatches) w.valid = false;
    if (cheat == Cheat::InfiniteItems)
        for (Watch &w : itemWatches) w.valid = false;

    g_cheatInvincible = IsEnabled(Cheat::Invincible) ? 1 : 0;
    g_cheatOneHitKill = IsEnabled(Cheat::OneHitKill) ? 1 : 0;
    g_cheatInfiniteOni = IsEnabled(Cheat::InfiniteOni) ? 1 : 0;
    g_cheatAlwaysIssen = IsEnabled(Cheat::AlwaysIssen) ? 1 : 0;
    g_cheatAutoChain = IsEnabled(Cheat::AutoChain) ? 1 : 0;
    g_cheatMoveSpeed = IsEnabled(Cheat::MoveSpeed) ? 1 : 0;

    std::cout << "Cheat " << i << (enabled[i] ? " on" : " off") << std::endl;
}

void Cheats::PollKeyboard()
{
    bool active = IsGameWindowActive();

    for (int i = 0; i < CHEAT_COUNT; i++) {
        bool down = active && (GetAsyncKeyState(KEYS[i]) & 0x8000) != 0;
        if (down && !keyDown[i])
            Toggle(static_cast<Cheat>(i));
        keyDown[i] = down;
    }
}

XINPUT_GAMEPAD Cheats::FilterPad(const XINPUT_GAMEPAD &pad)
{
    XINPUT_GAMEPAD filtered = pad;

    if (pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        WORD pressed = pad.wButtons & ~lastPadButtons;
        for (int i = 0; i < CHEAT_COUNT; i++)
            if (pressed & COMBO_BUTTONS[i])
                Toggle(static_cast<Cheat>(i));
        filtered.wButtons &= ~COMBO_MASK;
    }
    lastPadButtons = pad.wButtons;
    return filtered;
}

// Restores any watched value that went down. When more than loadThreshold values change
// at once (a save was loaded, a new chapter started) the new values are accepted instead.
void Cheats::KeepFromDecreasing(std::vector<Watch> &watches, size_t loadThreshold)
{
    size_t decreased = 0;

    for (Watch &w : watches)
        if (w.valid && ReadValue(w.address, w.size) < w.last)
            decreased++;

    bool reload = decreased > loadThreshold;
    for (Watch &w : watches) {
        DWORD value = ReadValue(w.address, w.size);
        if (w.valid && value < w.last && !reload)
            WriteValue(w.address, w.size, w.last);
        else
            w.last = value;
        w.valid = true;
    }
}

void Cheats::Tick()
{
    PollKeyboard();

    if (IsEnabled(Cheat::Invincible)) {
        // Damage to the player is already cancelled in DamageStub; this also undoes other losses.
        LONG maxHp = static_cast<LONG>(ReadValue(PLAYER_MAX_HP, 4));
        LONG hp = static_cast<LONG>(ReadValue(PLAYER_HP, 4));
        if (maxHp > 0 && maxHp <= 100000 && hp < maxHp)
            WriteValue(PLAYER_HP, 4, static_cast<DWORD>(maxHp));
    }
    if (IsEnabled(Cheat::InfiniteOni))
        KeepFromDecreasing(soulWatches, 2);
    if (IsEnabled(Cheat::InfiniteItems))
        KeepFromDecreasing(itemWatches, 2);
    if (IsEnabled(Cheat::AutoChain) && ReadValue(CHAINS_LEFT, 1) < CHAINS_KEPT)
        WriteValue(CHAINS_LEFT, 1, CHAINS_KEPT);
}
