#include "CrashLogger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#  include <windows.h>
#endif

// ---- Buffer pré-alocado estático (seguro para uso em signal handlers) ----

static constexpr int kMaxCrumbs = 30;
static constexpr int kCrumbSize = 160;

static char g_crumbs[kMaxCrumbs][kCrumbSize];
static std::atomic<int> g_head{0};   // próximo slot a escrever (cresce sem limite)
static std::atomic<int> g_count{0};  // total de escritas (cresce sem limite)

static char g_logPath[1024] = {};
static char g_version[64]   = {};

// ---- Escrita do relatório de crash ----

static void dumpCrashReport(const char* reason, unsigned long exCode = 0, void* exAddr = nullptr)
{
    if (!g_logPath[0]) return;

    FILE* f = fopen(g_logPath, "a");
    if (!f) return;

    time_t now = time(nullptr);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(f, "\n========== CRASH %s ==========\n", timebuf);
    fprintf(f, "Motivo  : %s\n", reason);
    if (exCode)
        fprintf(f, "Codigo  : 0x%08lX  Endereco: %p\n", exCode, exAddr);
    fprintf(f, "Versao  : %s\n", g_version);

    const int count = g_count.load(std::memory_order_relaxed);
    const int head  = g_head.load(std::memory_order_relaxed);
    const int total = count < kMaxCrumbs ? count : kMaxCrumbs;
    const int start = count < kMaxCrumbs ? 0 : (head % kMaxCrumbs);

    fprintf(f, "--- Ultimas acoes (%d) ---\n", total);
    for (int i = 0; i < total; ++i) {
        const int idx = (start + i) % kMaxCrumbs;
        fprintf(f, "  [%2d] %s\n", i + 1, g_crumbs[idx]);
    }
    fprintf(f, "=========== FIM ===========\n");
    fflush(f);
    fclose(f);
}

// ---- Handlers ----

#ifdef _WIN32
static LONG WINAPI sehHandler(EXCEPTION_POINTERS* ep)
{
    char reason[64];
    snprintf(reason, sizeof(reason), "SEH 0x%08lX",
             ep->ExceptionRecord->ExceptionCode);
    dumpCrashReport(reason,
                    ep->ExceptionRecord->ExceptionCode,
                    ep->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_CONTINUE_SEARCH;
}

// O runtime do MinGW instala seu proprio filtro de excecao (mais interno que o
// SetUnhandledExceptionFilter abaixo) que traduz access violation/etc. para um
// signal() (SIGSEGV) antes de chegar no handler acima — por isso o SEH nunca
// pegava o endereco real. Um Vectored Exception Handler roda ANTES dessa
// traducao, entao capturamos codigo+endereco aqui e deixamos a excecao seguir
// seu curso normal (CONTINUE_SEARCH) pro posixSignalHandler ainda rodar depois.
static LONG WINAPI vehHandler(EXCEPTION_POINTERS* ep)
{
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR: {
        char reason[64];
        snprintf(reason, sizeof(reason), "VEH 0x%08lX", code);
        dumpCrashReport(reason, code, ep->ExceptionRecord->ExceptionAddress);
        break;
    }
    default:
        break; // demais codigos (ex.: excecoes C++ internas) nao sao fatais — ignora
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

static void posixSignalHandler(int sig)
{
    const char* name = "UNKNOWN";
    switch (sig) {
    case SIGSEGV: name = "SIGSEGV"; break;
    case SIGABRT: name = "SIGABRT"; break;
    case SIGFPE:  name = "SIGFPE";  break;
    case SIGILL:  name = "SIGILL";  break;
    }
    dumpCrashReport(name);
    signal(sig, SIG_DFL);
    raise(sig);
}

static void qtMessageHandler(QtMsgType type, const QMessageLogContext& /*ctx*/, const QString& msg)
{
    if (type == QtFatalMsg || type == QtCriticalMsg) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Qt%s: %s",
                 type == QtFatalMsg ? "Fatal" : "Critical",
                 msg.toUtf8().constData());
        dumpCrashReport(buf);
    }
    fprintf(stderr, "%s\n", msg.toUtf8().constData());
}

// ---- API pública ----

void CrashLogger::install()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/crash.log");

    const QByteArray pathUtf = path.toUtf8();
    strncpy(g_logPath, pathUtf.constData(), sizeof(g_logPath) - 1);

    const QByteArray ver = QCoreApplication::applicationVersion().toUtf8();
    strncpy(g_version, ver.constData(), sizeof(g_version) - 1);

    // Rotaciona o log se ultrapassar 2 MB
    if (QFileInfo(path).size() > 2 * 1024 * 1024) {
        QFile::remove(path + QStringLiteral(".bak"));
        QFile::rename(path, path + QStringLiteral(".bak"));
    }

    {
        FILE* f = fopen(g_logPath, "a");
        if (f) {
            time_t now = time(nullptr);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
            fprintf(f, "\n[SESSAO INICIADA %s — v%s]\n", timebuf, g_version);
            fclose(f);
        }
    }

    signal(SIGSEGV, posixSignalHandler);
    signal(SIGABRT, posixSignalHandler);
    signal(SIGFPE,  posixSignalHandler);
    signal(SIGILL,  posixSignalHandler);

#ifdef _WIN32
    AddVectoredExceptionHandler(1, vehHandler); // 1 = primeiro da cadeia
    SetUnhandledExceptionFilter(sehHandler);    // rede de seguranca, caso o VEH nao capture
#endif

    qInstallMessageHandler(qtMessageHandler);
}

void CrashLogger::log(const char* action)
{
    const int slot = g_head.fetch_add(1, std::memory_order_relaxed) % kMaxCrumbs;
    strncpy(g_crumbs[slot], action, kCrumbSize - 1);
    g_crumbs[slot][kCrumbSize - 1] = '\0';
    g_count.fetch_add(1, std::memory_order_relaxed);
}

void CrashLogger::log(const QString& action)
{
    log(action.toUtf8().constData());
}

QString CrashLogger::logFilePath()
{
    return QString::fromUtf8(g_logPath);
}
