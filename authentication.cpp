#if defined(HIDE_PASSWORD) && !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>
#ifdef HIDE_PASSWORD
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif
#endif
#define USER_FILE        "users.dat"
#define BACKUP_FILE      "users.dat.bak"
#define TEMP_FILE        "users.dat.tmp"
#define FILE_HEADER      "IMF_AUTH_V1"
#define MAX_USERS        100
#define MAX_USERNAME     20
#define MIN_USERNAME     3
#define MAX_PASSWORD     32
#define MIN_PASSWORD     8
#define INPUT_BUF_SIZE   64
#define LINE_BUF_SIZE    256
#define MAX_ATTEMPTS     3
#define MAX_EMPLOYEE_ID  999999
#define MAX_MENU         16
#define SALT_BYTES       16
#define HASH_BYTES       32
#define SALT_HEX_LEN     (SALT_BYTES * 2)
#define HASH_HEX_LEN     (HASH_BYTES * 2)
#define PBKDF2_ITERATIONS 50000u
#define HMAC_MAX_MSG     32
#define DEMO_ADMIN_NAME      "admin"
#define DEMO_ADMIN_PASSWORD  "Admin@123"
typedef enum {
    ROLE_ADMIN    = 1,
    ROLE_EMPLOYEE = 2,
    ROLE_MANAGER  = 3
} UserRole;
typedef enum {
    PERM_SELF_SERVICE,
    PERM_MODULE_EMPLOYEE,
    PERM_MODULE_ATTENDANCE_ALL,
    PERM_MODULE_LEAVE_APPROVE,
    PERM_MODULE_PAYROLL_ALL,
    PERM_MODULE_DEPARTMENT,
    PERM_MANAGE_ACCOUNTS
} Permission;
typedef enum {
    ACT_SESSION_INFO, ACT_CHANGE_PASSWORD, ACT_SELF_SERVICE,
    ACT_MOD_EMPLOYEE, ACT_MOD_ATTENDANCE, ACT_MOD_LEAVE,
    ACT_MOD_PAYROLL, ACT_MOD_DEPARTMENT, ACT_ACCOUNTS
} Action;
typedef enum { AUTH_OK, AUTH_BAD_CREDENTIALS, AUTH_LOCKED, AUTH_DISABLED } AuthResult;
typedef enum { LOAD_OK, LOAD_MISSING, LOAD_BAD_HEADER, LOAD_EMPTY } LoadStatus;
typedef struct {
    char     username[MAX_USERNAME + 1];
    int      employeeId;
    UserRole role;
    int      active;
    int      mustChange;
    char     saltHex[SALT_HEX_LEN + 1];
    char     hashHex[HASH_HEX_LEN + 1];
    int      failedCount;
} User;
typedef struct {
    const char *label;
    Permission  perm;
    Action      action;
} MenuItem;
static User users[MAX_USERS];
static int  userCount = 0;
static const MenuItem menuItems[] = {
    { "View my session information",                 PERM_SELF_SERVICE,          ACT_SESSION_INFO    },
    { "Change my password",                          PERM_SELF_SERVICE,          ACT_CHANGE_PASSWORD },
    { "My attendance, leave and payroll",            PERM_SELF_SERVICE,          ACT_SELF_SERVICE    },
    { "Employee Management",                         PERM_MODULE_EMPLOYEE,       ACT_MOD_EMPLOYEE    },
    { "Attendance Management (all employees)",       PERM_MODULE_ATTENDANCE_ALL, ACT_MOD_ATTENDANCE  },
    { "Leave Management (approvals)",                PERM_MODULE_LEAVE_APPROVE,  ACT_MOD_LEAVE       },
    { "Payroll Management (all employees)",          PERM_MODULE_PAYROLL_ALL,    ACT_MOD_PAYROLL     },
    { "Department Management",                       PERM_MODULE_DEPARTMENT,     ACT_MOD_DEPARTMENT  },
    { "Account management (users)",                  PERM_MANAGE_ACCOUNTS,       ACT_ACCOUNTS        }
};
#define MENU_COUNT          ((int)(sizeof(menuItems) / sizeof(menuItems[0])))
#define ACCOUNTS_ITEM_INDEX 8
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static void sha256Compress(uint32_t st[8], const uint8_t blk[64])
{
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2, s0, s1;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)blk[i * 4] << 24) | ((uint32_t)blk[i * 4 + 1] << 16) |
               ((uint32_t)blk[i * 4 + 2] << 8) | (uint32_t)blk[i * 4 + 3];}
    for (i = 16; i < 64; i++) {
        s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;}
    a = st[0]; b = st[1]; c = st[2]; d = st[3];
    e = st[4]; f = st[5]; g = st[6]; h = st[7];
    for (i = 0; i < 64; i++) {
        t1 = h + (ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        t2 = (ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;}
    st[0] += a; st[1] += b; st[2] += c; st[3] += d;
    st[4] += e; st[5] += f; st[6] += g; st[7] += h;}
static void sha256(const uint8_t *msg, size_t len, uint8_t out[32])
{
    uint32_t st[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    uint8_t  tail[128];
    uint64_t bits = (uint64_t)len * 8u;
    size_t   i, rem, tailLen;
    int      j;
    for (i = 0; i + 64 <= len; i += 64) {
        sha256Compress(st, msg + i);}
    rem = len - i;
    memset(tail, 0, sizeof(tail));
    if (rem > 0) {
        memcpy(tail, msg + i, rem);
    }
    tail[rem] = 0x80;
    tailLen = (rem < 56) ? 64 : 128;
    for (j = 0; j < 8; j++) {
        tail[tailLen - 1 - (size_t)j] = (uint8_t)(bits >> (8 * j));
    }
    sha256Compress(st, tail);
    if (tailLen == 128) {
        sha256Compress(st, tail + 64);
    }
    for (j = 0; j < 8; j++) {
        out[j * 4]     = (uint8_t)(st[j] >> 24);
        out[j * 4 + 1] = (uint8_t)(st[j] >> 16);
        out[j * 4 + 2] = (uint8_t)(st[j] >> 8);
        out[j * 4 + 3] = (uint8_t)(st[j]);
    }
}

static void hmacSha256(const uint8_t *key, size_t keyLen,
                       const uint8_t *msg, size_t msgLen, uint8_t out[32])
{
    uint8_t k[64], buf[64 + HMAC_MAX_MSG], inner[32];
    size_t  i;

    memset(k, 0, sizeof(k));
    if (keyLen > 64) {
        sha256(key, keyLen, k);
    } else if (keyLen > 0) {
        memcpy(k, key, keyLen);
    }
    for (i = 0; i < 64; i++) buf[i] = (uint8_t)(k[i] ^ 0x36);
    memcpy(buf + 64, msg, msgLen);
    sha256(buf, 64 + msgLen, inner);

    for (i = 0; i < 64; i++) buf[i] = (uint8_t)(k[i] ^ 0x5c);
    memcpy(buf + 64, inner, 32);
    sha256(buf, 96, out);
}

static void pbkdf2(const char *password, const uint8_t salt[SALT_BYTES],
                   uint32_t iterations, uint8_t out[HASH_BYTES])
{
    uint8_t  msg[SALT_BYTES + 4], u[32], t[32];
    size_t   pwLen = strlen(password), j;
    uint32_t i;

    memcpy(msg, salt, SALT_BYTES);
    msg[SALT_BYTES] = 0; msg[SALT_BYTES + 1] = 0;
    msg[SALT_BYTES + 2] = 0; msg[SALT_BYTES + 3] = 1;

    hmacSha256((const uint8_t *)password, pwLen, msg, sizeof(msg), u);
    memcpy(t, u, 32);
    for (i = 1; i < iterations; i++) {
        hmacSha256((const uint8_t *)password, pwLen, u, 32, u);
        for (j = 0; j < 32; j++) t[j] ^= u[j];
    }
    memcpy(out, t, HASH_BYTES);
}

static void wipe(void *p, size_t n)
{
    volatile unsigned char *v = (volatile unsigned char *)p;
    while (n--) *v++ = 0;
}

static void bytesToHex(const uint8_t *in, size_t n, char *out)
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) {
        out[i * 2]     = digits[in[i] >> 4];
        out[i * 2 + 1] = digits[in[i] & 0x0F];
    }
    out[n * 2] = '\0';
}

static int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int isHexString(const char *s, size_t expectedLen)
{
    size_t i;
    if (strlen(s) != expectedLen) return 0;
    for (i = 0; i < expectedLen; i++) {
        if (hexVal(s[i]) < 0) return 0;
    }
    return 1;
}

static void hexToBytes(const char *hex, uint8_t *out, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        out[i] = (uint8_t)((hexVal(hex[i * 2]) << 4) | hexVal(hex[i * 2 + 1]));
    }
}

static int constantTimeEquals(const char *a, const char *b, size_t n)
{
    unsigned char diff = 0;
    size_t i;
    for (i = 0; i < n; i++) diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

static int fillRandom(uint8_t *buf, size_t n)
{
    static int seeded = 0;
    size_t i;
    FILE *f = fopen("/dev/urandom", "rb");

    if (f != NULL) {
        size_t got = fread(buf, 1, n, f);
        fclose(f);
        if (got == n) return 1;
    }
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = 1;
    }
    for (i = 0; i < n; i++) buf[i] = (uint8_t)((rand() >> 4) & 0xFF);
    return 0;
}

static int equalsIgnoreCase(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static int isAllDigits(const char *s)
{
    if (*s == '\0') return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int copyFile(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb"), *out;
    char buf[512];
    size_t n;
    int ok = 1;

    if (in == NULL) return 0;
    out = fopen(dst, "wb");
    if (out == NULL) { fclose(in); return 0; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    }
    if (ferror(in)) ok = 0;
    if (fclose(out) != 0) ok = 0;
    fclose(in);
    return ok;
}

static int readLine(const char *prompt, char *buf, size_t size)
{
    size_t len;
    int c;

    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, (int)size, stdin) == NULL) return 0;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        if (len > 1 && buf[len - 2] == '\r') buf[len - 2] = '\0';
        return 1;
    }
    while ((c = getchar()) != '\n' && c != EOF) { }
    return -1;
}

#ifndef HIDE_PASSWORD

static int readPassword(const char *prompt, char *buf, size_t size)
{
    return readLine(prompt, buf, size);
}

#elif defined(_WIN32)

static int readPassword(const char *prompt, char *buf, size_t size)
{
    size_t len = 0;
    int c, tooLong = 0;

    printf("%s", prompt);
    fflush(stdout);
    while ((c = _getch()) != '\r' && c != '\n') {
        if (c == 3) { buf[0] = '\0'; printf("\n"); return 0; }
        if (c == 0 || c == 224) { (void)_getch(); continue; }
        if (c == '\b') { if (len > 0) len--; continue; }
        if (len < size - 1) buf[len++] = (char)c; else tooLong = 1;
    }
    buf[len] = '\0';
    printf("\n");
    return tooLong ? -1 : 1;
}

#else

static int readPassword(const char *prompt, char *buf, size_t size)
{
    struct termios oldt, newt;
    int status;

    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &oldt) != 0) {
        return readLine(prompt, buf, size);
    }
    newt = oldt;
    newt.c_lflag &= ~(tcflag_t)ECHO;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &newt) != 0) {
        return readLine(prompt, buf, size);
    }
    status = readLine(prompt, buf, size);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldt);
    printf("\n");
    return status;
}

#endif

static int readMenuChoice(const char *prompt)
{
    char buf[INPUT_BUF_SIZE], *end;
    long value;
    int status = readLine(prompt, buf, sizeof(buf));

    if (status == 0) return -2;
    if (status == -1 || buf[0] == '\0') return -1;
    value = strtol(buf, &end, 10);
    if (*end != '\0' || value < 0 || value > 99) return -1;
    return (int)value;
}

static int readInt(const char *prompt, long min, long max, int *out)
{
    char buf[INPUT_BUF_SIZE], *end;
    long v;

    if (readLine(prompt, buf, sizeof(buf)) != 1 || buf[0] == '\0') return 0;
    v = strtol(buf, &end, 10);
    if (*end != '\0' || v < min || v > max) return 0;
    *out = (int)v;
    return 1;
}

static int isValidUsername(const char *name)
{
    size_t i, len = strlen(name);

    if (len < MIN_USERNAME || len > MAX_USERNAME) return 0;
    if (!isalpha((unsigned char)name[0])) return 0;
    for (i = 1; i < len; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') return 0;
    }
    return 1;
}

static int checkPasswordPolicy(const char *pw, const char **reason)
{
    size_t i, len = strlen(pw);
    int hasLetter = 0, hasDigit = 0;

    if (len < MIN_PASSWORD || len > MAX_PASSWORD) {
        *reason = "Password must be 8-32 characters long.";
        return 0;
    }
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)pw[i];
        if (!isprint(c) || isspace(c)) {
            *reason = "Password must not contain spaces or non-printable characters.";
            return 0;
        }
        if (isalpha(c)) hasLetter = 1;
        if (isdigit(c)) hasDigit = 1;
    }
    if (!hasLetter || !hasDigit) {
        *reason = "Password must contain at least one letter and one digit.";
        return 0;
    }
    return 1;
}

static int isValidLoginToken(const char *text, size_t maxLen)
{
    size_t i, len = strlen(text);
    if (len == 0 || len > maxLen) return 0;
    for (i = 0; i < len; i++) {
        if (!isprint((unsigned char)text[i]) || isspace((unsigned char)text[i])) return 0;
    }
    return 1;
}
static const char *roleToString(UserRole role)
{
    switch (role) {
        case ROLE_ADMIN:    return "Administrator";
        case ROLE_EMPLOYEE: return "Employee";
        case ROLE_MANAGER:  return "Manager";
    }
    return "Unknown";
}
static int findByUsername(const char *name)
{
    int i;
    for (i = 0; i < userCount; i++) {
        if (equalsIgnoreCase(users[i].username, name)) return i;
    }
    return -1;
}
static int findByEmployeeId(int id)
{
    int i;
    if (id <= 0) return -1;
    for (i = 0; i < userCount; i++) {
        if (users[i].employeeId == id) return i;
    }
    return -1;
}
static int findByIdentifier(const char *text)
{
    if (isAllDigits(text)) {
        if (strlen(text) > 7) return -1;
        return findByEmployeeId(atoi(text));
    }
    return findByUsername(text);
}

static int countActiveAdmins(void)
{
    int i, n = 0;
    for (i = 0; i < userCount; i++) {
        if (users[i].role == ROLE_ADMIN && users[i].active) n++;
    }
    return n;
}
static void makePasswordRecord(const char *password,
                               char saltHex[SALT_HEX_LEN + 1],
                               char hashHex[HASH_HEX_LEN + 1])
{
    uint8_t salt[SALT_BYTES], hash[HASH_BYTES];
    fillRandom(salt, sizeof(salt));
    pbkdf2(password, salt, PBKDF2_ITERATIONS, hash);
    bytesToHex(salt, sizeof(salt), saltHex);
    bytesToHex(hash, sizeof(hash), hashHex);
    wipe(hash, sizeof(hash));
}
static int verifyPassword(const User *user, const char *password)
{
    uint8_t salt[SALT_BYTES], hash[HASH_BYTES];
    char hex[HASH_HEX_LEN + 1];
    int ok;
    hexToBytes(user->saltHex, salt, sizeof(salt));
    pbkdf2(password, salt, PBKDF2_ITERATIONS, hash);
    bytesToHex(hash, sizeof(hash), hex);
    ok = constantTimeEquals(hex, user->hashHex, HASH_HEX_LEN);
    wipe(hash, sizeof(hash));
    wipe(hex, sizeof(hex));
    return ok;
}
static AuthResult authenticateUser(const char *identifier, const char *password, int *outIdx)
{
    int idx = findByIdentifier(identifier);
    User *u;

    if (idx < 0) return AUTH_BAD_CREDENTIALS;
    u = &users[idx];
    if (u->failedCount >= MAX_ATTEMPTS) return AUTH_LOCKED;
    if (!verifyPassword(u, password)) {
        u->failedCount++;
        return AUTH_BAD_CREDENTIALS;
    }
    u->failedCount = 0;
    if (!u->active) return AUTH_DISABLED;
    *outIdx = idx;
    return AUTH_OK;}
static int isAuthorized(const User *user, Permission perm)
{
    if (user == NULL || !user->active) return 0;
    switch (perm) {
        case PERM_SELF_SERVICE:
            return 1;
        case PERM_MODULE_ATTENDANCE_ALL:
        case PERM_MODULE_LEAVE_APPROVE:
        case PERM_MODULE_DEPARTMENT:
            return user->role == ROLE_ADMIN || user->role == ROLE_MANAGER;
        case PERM_MODULE_EMPLOYEE:
        case PERM_MODULE_PAYROLL_ALL:
        case PERM_MANAGE_ACCOUNTS:
            return user->role == ROLE_ADMIN;
    }
    return 0;
}
static int requirePermission(const User *user, Permission perm)
{
    if (isAuthorized(user, perm)) return 1;
    printf("\n[ACCESS DENIED] Your role (%s) is not authorized for this function.\n",
           user != NULL ? roleToString(user->role) : "none");
    return 0;
}
static int saveUsers(void)
{
    FILE *f = fopen(TEMP_FILE, "w");
    int i;
    if (f == NULL) return 0;
    fprintf(f, "%s\n", FILE_HEADER);
    for (i = 0; i < userCount; i++) {
        fprintf(f, "%s %d %d %d %d %s %s\n",
                users[i].username, users[i].employeeId, (int)users[i].role,
                users[i].active, users[i].mustChange,
                users[i].saltHex, users[i].hashHex);
    }
    if (fflush(f) != 0 || ferror(f)) {
        fclose(f); remove(TEMP_FILE);
        return 0;
    }
    if (fclose(f) != 0) { remove(TEMP_FILE); return 0; }
    remove(USER_FILE);
    return rename(TEMP_FILE, USER_FILE) == 0;}
static int parseUserLine(const char *line)
{
    User u;
    int emp, role, active, must;
    char salt[SALT_HEX_LEN + 1], hash[HASH_HEX_LEN + 1];
    memset(&u, 0, sizeof(u));
    if (userCount >= MAX_USERS) return 0;
    if (sscanf(line, "%20s %d %d %d %d %32s %64s",
               u.username, &emp, &role, &active, &must, salt, hash) != 7) return 0;
    if (!isValidUsername(u.username)) return 0;
    if (emp < 0 || emp > MAX_EMPLOYEE_ID) return 0;
    if (role < ROLE_ADMIN || role > ROLE_MANAGER) return 0;
    if ((active != 0 && active != 1) || (must != 0 && must != 1)) return 0;
    if (!isHexString(salt, SALT_HEX_LEN) || !isHexString(hash, HASH_HEX_LEN)) return 0;
    if (role != ROLE_ADMIN && emp == 0) return 0;
    if (findByUsername(u.username) >= 0) return 0;
    if (emp > 0 && findByEmployeeId(emp) >= 0) return 0;
    u.employeeId = emp;
    u.role = (UserRole)role;
    u.active = active;
    u.mustChange = must;
    memcpy(u.saltHex, salt, sizeof(salt));
    memcpy(u.hashHex, hash, sizeof(hash));
    users[userCount++] = u;
    return 1;
}
static LoadStatus loadUsers(int *skipped)
{
    FILE *f = fopen(USER_FILE, "r");
    char line[LINE_BUF_SIZE];
    size_t len;
    int c;
    if (f == NULL) return LOAD_MISSING;
    if (fgets(line, sizeof(line), f) == NULL) { fclose(f); return LOAD_BAD_HEADER; }
    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
    if (strcmp(line, FILE_HEADER) != 0) { fclose(f); return LOAD_BAD_HEADER; }
    while (fgets(line, sizeof(line), f) != NULL) {
        len = strlen(line);
        if (len > 0 && line[len - 1] != '\n' && !feof(f)) {
            while ((c = fgetc(f)) != '\n' && c != EOF) { }
            (*skipped)++;
            continue;
        }
        if (line[strspn(line, " \t\r\n")] == '\0') continue;
        if (!parseUserLine(line)) (*skipped)++;
    }
    fclose(f);
    return userCount > 0 ? LOAD_OK : LOAD_EMPTY;
}
static int createDemoAdmin(void)
{
    User u;
    memset(&u, 0, sizeof(u));
    snprintf(u.username, sizeof(u.username), "%s", DEMO_ADMIN_NAME);
    u.role = ROLE_ADMIN;
    u.active = 1;
    u.mustChange = 1;
    makePasswordRecord(DEMO_ADMIN_PASSWORD, u.saltHex, u.hashHex);
    users[userCount++] = u;
    printf("Demo administrator '%s' created. Its documented demo password must be\n"
           "changed at first login.\n", DEMO_ADMIN_NAME);
    return saveUsers();
}
static int initUsers(void)
{
    int skipped = 0, needBackup = 0, needDemo = 0;

    switch (loadUsers(&skipped)) {
        case LOAD_MISSING:
            printf("No user file found. First run: setting up the demo administrator.\n");
            needDemo = 1;
            break;
        case LOAD_BAD_HEADER:
            printf("WARNING: '%s' is not a valid IMF credential file.\n", USER_FILE);
            needBackup = needDemo = 1;
            break;
        case LOAD_EMPTY:
            printf("WARNING: '%s' contains no valid accounts.\n", USER_FILE);
            needBackup = needDemo = 1;
            break;
        case LOAD_OK:
            if (skipped > 0) {
                printf("WARNING: %d invalid line(s) in '%s' were ignored.\n", skipped, USER_FILE);
                needBackup = 1;
            }
            if (countActiveAdmins() == 0) {
                printf("WARNING: no active administrator found. Starting recovery.\n");
                needBackup = needDemo = 1;
            }
            break;
    }
    if (needBackup) {
        if (!copyFile(USER_FILE, BACKUP_FILE)) {
            printf("ERROR: could not back up '%s'. Refusing to overwrite it.\n", USER_FILE);
            return 0;
        }
        printf("A backup copy was saved as '%s'.\n", BACKUP_FILE);
    }
    if (needDemo) {
        userCount = 0;
        if (!createDemoAdmin()) {
            printf("WARNING: could not write '%s'. Changes will last only for this run.\n", USER_FILE);
        }
    } else if (needBackup && !saveUsers()) {
        printf("WARNING: could not rewrite '%s'.\n", USER_FILE);
    }
    return 1;
}
static int changePassword(int idx, int forced)
{
    char current[INPUT_BUF_SIZE], pw1[INPUT_BUF_SIZE], pw2[INPUT_BUF_SIZE];
    const char *reason = NULL;
    User *u = &users[idx];
    int tries, status, changed = 0;
    pw1[0] = pw2[0] = '\0';
    if (!forced) {
        status = readPassword("Current password: ", current, sizeof(current));
        if (status != 1) current[0] = '\0';
        printf("Checking password...\n");
        fflush(stdout);
        if (status != 1 || !verifyPassword(u, current)) {
            printf("Password not changed: current password is incorrect.\n");
            wipe(current, sizeof(current));
            return 0;
        }
        wipe(current, sizeof(current));
    }
    printf("Password rules: 8-32 characters, no spaces, at least one letter and one digit.\n");
    for (tries = 0; tries < MAX_ATTEMPTS && !changed; tries++) {
        status = readPassword("New password: ", pw1, sizeof(pw1));
        if (status == 0) break;
        if (status == -1) { printf("Password too long.\n"); continue; }
        if (!checkPasswordPolicy(pw1, &reason)) { printf("%s\n", reason); continue; }
        if (verifyPassword(u, pw1)) {
            printf("The new password must be different from the current one.\n");
            continue;
        }
        status = readPassword("Confirm new password: ", pw2, sizeof(pw2));
        if (status != 1 || strcmp(pw1, pw2) != 0) {
            printf("Passwords did not match.\n");
            continue;
        }
        makePasswordRecord(pw1, u->saltHex, u->hashHex);
        u->mustChange = 0;
        changed = 1;
    }
    wipe(pw1, sizeof(pw1));
    wipe(pw2, sizeof(pw2));

    if (!changed) {
        printf("Password not changed.\n");
        return 0;
    }
    if (!saveUsers()) {
        printf("WARNING: password changed for this run but could not be saved to '%s'.\n", USER_FILE);
    } else {
        printf("Password changed successfully.\n");
    }
    return 1;
}
static void addUser(const User *actor)
{
    char name[INPUT_BUF_SIZE], pw1[INPUT_BUF_SIZE], pw2[INPUT_BUF_SIZE];
    const char *reason = NULL;
    User nu;
    int role, emp = 0;
    pw1[0] = pw2[0] = '\0';
    if (!requirePermission(actor, PERM_MANAGE_ACCOUNTS)) return;
    if (userCount >= MAX_USERS) { printf("User limit (%d) reached.\n", MAX_USERS); return; }

    if (readLine("New username (3-20 chars, starts with a letter, letters/digits/_): ",
                 name, sizeof(name)) != 1 || !isValidUsername(name)) {
        printf("Invalid username.\n");
        return;
    }
    if (findByUsername(name) >= 0) {
        printf("That username already exists (usernames are not case-sensitive).\n");
        return;
    }
    if (!readInt("Role (1 = Administrator, 2 = Employee, 3 = Manager): ", 1, 3, &role)) {
        printf("Invalid role.\n");
        return;
    }
    if (role == ROLE_ADMIN) {
        if (!readInt("Employee ID (0 = none): ", 0, MAX_EMPLOYEE_ID, &emp)) {
            printf("Invalid Employee ID.\n");
            return;
        }
    } else if (!readInt("Employee ID (1-999999): ", 1, MAX_EMPLOYEE_ID, &emp)) {
        printf("Invalid Employee ID.\n");
        return;
    }
    if (emp > 0 && findByEmployeeId(emp) >= 0) {
        printf("That Employee ID already has an account.\n");
        return;
    }

    printf("Set a TEMPORARY password. The user must change it at first login.\n");
    if (readPassword("Temporary password: ", pw1, sizeof(pw1)) != 1) {
        printf("Invalid password input.\n");
        wipe(pw1, sizeof(pw1));
        return;
    }
    if (!checkPasswordPolicy(pw1, &reason)) {
        printf("%s\n", reason);
        wipe(pw1, sizeof(pw1));
        return;
    }
    if (readPassword("Confirm password: ", pw2, sizeof(pw2)) != 1 || strcmp(pw1, pw2) != 0) {
        printf("Passwords did not match. Account not created.\n");
        wipe(pw1, sizeof(pw1)); wipe(pw2, sizeof(pw2));
        return;
    }
    memset(&nu, 0, sizeof(nu));
    snprintf(nu.username, sizeof(nu.username), "%s", name);
    nu.employeeId = emp;
    nu.role = (UserRole)role;
    nu.active = 1;
    nu.mustChange = 1;
    makePasswordRecord(pw1, nu.saltHex, nu.hashHex);
    wipe(pw1, sizeof(pw1)); wipe(pw2, sizeof(pw2));
    users[userCount++] = nu;
    if (!saveUsers()) {
        userCount--;
        printf("ERROR: could not save to '%s'. Account NOT created.\n", USER_FILE);
        return;
    }
    printf("Account '%s' created (%s).\n", nu.username, roleToString(nu.role));
}
static void viewAccounts(const User *actor)
{
    int i;
    const char *status;
    if (!requirePermission(actor, PERM_MANAGE_ACCOUNTS)) return;
    printf("\n%-4s %-20s %-8s %-14s %-9s %s\n", "No.", "Username", "Emp ID", "Role", "Status", "Password");
    for (i = 0; i < userCount; i++) {
        status = !users[i].active ? "Disabled"
               : users[i].failedCount >= MAX_ATTEMPTS ? "Locked" : "Active";
        printf("%-4d %-20s ", i + 1, users[i].username);
        if (users[i].employeeId > 0) printf("%-8d ", users[i].employeeId);
        else                         printf("%-8s ", "-");
        printf("%-14s %-9s %s\n", roleToString(users[i].role), status,
               users[i].mustChange ? "change required" : "set");
    }
    printf("Total accounts: %d\n", userCount);
}
static int promptForUser(void)
{
    char name[INPUT_BUF_SIZE];
    int idx;
    if (readLine("Username: ", name, sizeof(name)) != 1 || name[0] == '\0') {
        printf("Invalid input.\n");
        return -1;
    }
    idx = findByUsername(name);
    if (idx < 0) printf("No such user.\n");
    return idx;
}
static void toggleAccountActive(const User *actor)
{
    int idx;
    if (!requirePermission(actor, PERM_MANAGE_ACCOUNTS)) return;
    idx = promptForUser();
    if (idx < 0) return;
    if (&users[idx] == actor) { printf("You cannot disable your own account.\n"); return; }
    if (users[idx].active) {
        if (users[idx].role == ROLE_ADMIN && countActiveAdmins() <= 1) {
            printf("Refused: this is the last active administrator.\n");
            return;
        }
        users[idx].active = 0;
        printf("Account '%s' disabled.\n", users[idx].username);
    } else {
        users[idx].active = 1;
        printf("Account '%s' enabled.\n", users[idx].username);
    }
    if (!saveUsers()) printf("WARNING: change not saved to '%s'.\n", USER_FILE);
}
static void resetUserPassword(const User *actor)
{
    char pw1[INPUT_BUF_SIZE], pw2[INPUT_BUF_SIZE];
    const char *reason = NULL;
    int idx;
    pw1[0] = pw2[0] = '\0';
    if (!requirePermission(actor, PERM_MANAGE_ACCOUNTS)) return;
    idx = promptForUser();
    if (idx < 0) return;
    if (&users[idx] == actor) {
        printf("Use 'Change my password' for your own account.\n");
        return;
    }
    if (readPassword("Temporary password: ", pw1, sizeof(pw1)) != 1) {
        printf("Invalid password input.\n");
        wipe(pw1, sizeof(pw1));
        return;
    }
    if (!checkPasswordPolicy(pw1, &reason)) {
        printf("%s\n", reason);
        wipe(pw1, sizeof(pw1));
        return;
    }
    if (readPassword("Confirm password: ", pw2, sizeof(pw2)) != 1 || strcmp(pw1, pw2) != 0) {
        printf("Passwords did not match. Nothing changed.\n");
        wipe(pw1, sizeof(pw1)); wipe(pw2, sizeof(pw2));
        return;
    }
    makePasswordRecord(pw1, users[idx].saltHex, users[idx].hashHex);
    wipe(pw1, sizeof(pw1)); wipe(pw2, sizeof(pw2));
    users[idx].mustChange = 1;
    users[idx].failedCount = 0;
    printf("Password reset for '%s'. They must change it at next login.\n", users[idx].username);
    if (!saveUsers()) printf("WARNING: change not saved to '%s'.\n", USER_FILE);
}
static void unlockAccount(const User *actor)
{
    int idx;
    if (!requirePermission(actor, PERM_MANAGE_ACCOUNTS)) return;
    idx = promptForUser();
    if (idx < 0) return;
    users[idx].failedCount = 0;
    printf("Account '%s' unlocked.\n", users[idx].username);
}
static void showMainMenu(void)
{
    printf("\n==============================================\n");
    printf("   IMF - Integrated Management Framework\n");
    printf("   Authentication Module\n");
    printf("==============================================\n");
    printf("1. Login\n");
    printf("0. Exit\n");
}
static void showSessionInfo(const User *user)
{
    printf("\n--- Session Information ---\n");
    printf("Username    : %s\n", user->username);
    printf("Role        : %s\n", roleToString(user->role));
    if (user->employeeId > 0) printf("Employee ID : %d\n", user->employeeId);
    else                      printf("Employee ID : N/A\n");}
static void showPlaceholder(const char *name)
{printf("\n[Placeholder] '%s' will be called from here after IMF integration.\n", name);}
static void showSelfService(const User *user)
{
    if (user->employeeId <= 0) {
        printf("\nThis account is not linked to an Employee ID, so there are no personal records.\n");
        return;
    }
    printf("\n[Placeholder] Attendance, leave and payroll for Employee ID %d\n"
           "will be shown here after integration. Pass user->employeeId to each module.\n",
           user->employeeId);
}
static void runAccountManagement(const User *actor)
{
    int choice;
    if (!requirePermission(actor, PERM_MANAGE_ACCOUNTS)) return;
    for (;;) {
        printf("\n------ ACCOUNT MANAGEMENT (Administrator only) ------\n");
        printf("1. Add user\n");
        printf("2. View accounts\n");
        printf("3. Enable / disable account\n");
        printf("4. Reset a user's password\n");
        printf("5. Unlock account\n");
        printf("0. Back\n");
        choice = readMenuChoice("Enter choice: ");
        switch (choice) {
            case 1:  addUser(actor);             break;
            case 2:  viewAccounts(actor);        break;
            case 3:  toggleAccountActive(actor); break;
            case 4:  resetUserPassword(actor);   break;
            case 5:  unlockAccount(actor);       break;
            case 0:
            case -2: return;
            default: printf("\nInvalid choice.\n");
        }
    }
}
static void performAction(const User *user, const MenuItem *item)
{
    if (!requirePermission(user, item->perm)) return;
    switch (item->action) {
        case ACT_SESSION_INFO:    showSessionInfo(user);                         break;
        case ACT_CHANGE_PASSWORD: changePassword((int)(user - users), 0);        break;
        case ACT_SELF_SERVICE:    showSelfService(user);                         break;
        case ACT_MOD_EMPLOYEE:
        case ACT_MOD_ATTENDANCE:
        case ACT_MOD_LEAVE:
        case ACT_MOD_PAYROLL:
        case ACT_MOD_DEPARTMENT:  showPlaceholder(item->label);                  break;
        case ACT_ACCOUNTS:        runAccountManagement(user);                    break;
    }
}
static void runDashboard(int idx)
{
    const User *user = &users[idx];
    int shown[MAX_MENU], count, choice, i, demoNumber;
    for (;;) {
        count = 0;
        printf("\n========== %s DASHBOARD ==========\n", roleToString(user->role));
        printf("Logged in as: %s\n", user->username);
        for (i = 0; i < MENU_COUNT; i++) {
            if (isAuthorized(user, menuItems[i].perm)) {
                shown[count++] = i;
                printf("%d. %s\n", count, menuItems[i].label);
            }
        }
        demoNumber = 0;
        if (!isAuthorized(user, PERM_MANAGE_ACCOUNTS)) {
            demoNumber = count + 1;
            printf("%d. [Demo] Try account management (admin only)\n", demoNumber);
        }
        printf("0. Logout\n");
        choice = readMenuChoice("Enter choice: ");
        if (choice == 0 || choice == -2) {
            printf("\nLogout successful. Goodbye, %s.\n", user->username);
            return;
        }
        if (choice >= 1 && choice <= count) {
            performAction(user, &menuItems[shown[choice - 1]]);
        } else if (demoNumber != 0 && choice == demoNumber) {
            performAction(user, &menuItems[ACCOUNTS_ITEM_INDEX]);
        } else {
            printf("\nInvalid choice.\n");
        }
    }
}
static void handleLogin(void)
{
    char ident[INPUT_BUF_SIZE], password[INPUT_BUF_SIZE];
    int attempt, status, idx = -1;
    AuthResult result;

    for (attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        printf("\n--- Login (attempt %d of %d) ---\n", attempt, MAX_ATTEMPTS);

        status = readLine("Username or Employee ID: ", ident, sizeof(ident));
        if (status == 0) return;
        if (status == -1 || !isValidLoginToken(ident, MAX_USERNAME)) {
            printf("Invalid input: 1-%d characters, no spaces.\n", MAX_USERNAME);
            continue;
        }
        status = readPassword("Password: ", password, sizeof(password));
        if (status == 0) return;
        if (status == -1 || !isValidLoginToken(password, MAX_PASSWORD)) {
            wipe(password, sizeof(password));
            printf("Invalid input: password must be 1-%d characters, no spaces.\n", MAX_PASSWORD);
            continue;
        }

        printf("Checking password...\n");
        fflush(stdout);
        result = authenticateUser(ident, password, &idx);
        wipe(password, sizeof(password));

        if (result == AUTH_OK) {
            printf("\nLogin successful. Welcome, %s!\n", users[idx].username);
            printf("Role identified: %s\n", roleToString(users[idx].role));
            if (users[idx].mustChange) {
                printf("\nYou must set a new password before continuing.\n");
                if (!changePassword(idx, 1)) {
                    printf("Logged out: a new password is required.\n");
                    return;
                }
            }
            runDashboard(idx);
            return;
        }
        if (result == AUTH_LOCKED) {
            printf("This account is locked after too many failed attempts.\n"
                   "Ask an administrator to unlock it, or restart the program.\n");
            return;
        }
        if (result == AUTH_DISABLED) {
            printf("This account has been disabled. Contact an administrator.\n");
            return;
        }
        printf("Login failed: invalid credentials.\n");
    }
    printf("\nToo many failed attempts. Returning to the main menu.\n");
}
static int runSelfTest(void)
{
    uint8_t out[32];
    char hex[65];
    int ok = 1;
    sha256((const uint8_t *)"abc", 3, out);
    bytesToHex(out, 32, hex);
    printf("SHA-256(\"abc\")  : %s\n", hex);
    if (strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0) ok = 0;
    hmacSha256((const uint8_t *)"Jefe", 4,
               (const uint8_t *)"what do ya want for nothing?", 28, out);
    bytesToHex(out, 32, hex);
    printf("HMAC (RFC 4231) : %s\n", hex);
    if (strcmp(hex, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843") != 0) ok = 0;
    printf("Self-test: %s\n", ok ? "PASSED" : "FAILED");
    return ok;}
int main(int argc, char *argv[])
{int choice;
    if (argc > 1 && strcmp(argv[1], "--selftest") == 0) {
        return runSelfTest() ? 0 : 1;
    }
    if (!initUsers()) {
        return 1;
    }
    for (;;) {
        showMainMenu();
        choice = readMenuChoice("Enter choice: ");
        switch (choice) {
            case 1:
                handleLogin();
                break;
            case 0:
            case -2:
                printf("\nExiting IMF Authentication Module. Goodbye.\n");
                return 0;
            default:
                printf("\nInvalid choice. Please enter 1 or 0.\n");
        }
    }
}
