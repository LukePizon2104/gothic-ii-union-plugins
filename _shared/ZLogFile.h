// Buforowany plik logu dla pluginow Union (zDismember, zTimeRewind).
// Dawniej fopen/fclose przy kazdej linii - na Windows z Defenderem kazde otwarcie potrafi trwac milisekundy (podejrzane
// o przycinki przy Debug=1). Teraz: plik otwierany raz, linie do bufora w pamieci, zapis na dysk w Flush() - raz na
// klatke (koniec Game_Loop), od razu przy duzym buforze (FLUSH_BYTES), przy wyjsciu (Game_Exit, destruktor przy
// zwalnianiu DLL) i przy wyjatku krytycznym (wektorowy handler - linie sprzed awarii nie przepadaja).
// Format linii bez zmian: "[%9.2f] tekst" i CRLF jak w trybie tekstowym fopen("a").
// Plik usuniety albo przeniesiony w trakcie gry (czyszczenie logu) jest zakladany od nowa - sprawdzane najwyzej raz
// na sekunde przy zapisie. Dzielony do odczytu i zapisu, wiec mozna go czytac i obcinac przy wlaczonej grze.
#pragma once
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

namespace zlogfile {
	const size_t FLUSH_BYTES = 32 * 1024;
	const ULONGLONG RECHECK_MS = 1000;
	const int MAX_FILES = 8;

	class LogFile;
	inline LogFile* g_files[MAX_FILES] = {};   // pliki do zapisu przy wyjatku krytycznym
	inline PVOID g_handler = nullptr;
	LONG CALLBACK CrashFlush(EXCEPTION_POINTERS* ep);

	class LogFile {
	public:
		explicit LogFile(const char* path) {
			InitializeCriticalSection(&cs_);
			lstrcpynA(path_, path, MAX_PATH);
		}
		~LogFile() {
			Unregister();
			EnterCriticalSection(&cs_);
			FlushLocked();
			if (h_ != INVALID_HANDLE_VALUE)
				CloseHandle(h_);
			h_ = INVALID_HANDLE_VALUE;
			LeaveCriticalSection(&cs_);
			DeleteCriticalSection(&cs_);
		}
		LogFile(const LogFile&) = delete;
		LogFile& operator=(const LogFile&) = delete;

		// Linia "[czas] tekst" do bufora (czas GetTickCount64 w chwili wywolania, jak dawniej).
		void WriteV(const char* fmt, va_list args) {
			char stamp[40];
			int ns = std::snprintf(stamp, sizeof(stamp), "[%9.2f] ", GetTickCount64() / 1000.0);
			char line[1024];   // nie "small" - to makro z rpcndr.h (windows.h)
			va_list copy;
			va_copy(copy, args);
			int n = std::vsnprintf(line, sizeof(line), fmt, copy);
			va_end(copy);
			std::string big;
			const char* text = line;
			if (n < 0)
				n = 0;
			else if (n >= (int)sizeof(line)) {
				big.resize((size_t)n + 1);
				std::vsnprintf(&big[0], big.size(), fmt, args);
				text = big.c_str();
			}
			EnterCriticalSection(&cs_);
			if (!registered_)
				Register();
			if (buf_.capacity() < FLUSH_BYTES * 2)
				buf_.reserve(FLUSH_BYTES * 2);
			if (ns > 0)
				buf_.append(stamp, (size_t)ns);
			if (!std::memchr(text, '\n', (size_t)n))
				buf_.append(text, (size_t)n);
			else
				for (int i = 0; i < n; i++) {
					if (text[i] == '\n')
						buf_ += '\r';
					buf_ += text[i];
				}
			buf_ += "\r\n";
			if (buf_.size() >= FLUSH_BYTES)
				FlushLocked();
			LeaveCriticalSection(&cs_);
		}

		void Flush() {
			EnterCriticalSection(&cs_);
			FlushLocked();
			LeaveCriticalSection(&cs_);
		}
		// Z handlera wyjatkow: bez czekania na blokade (inny watek moze ja trzymac).
		bool TryFlush() {
			if (!TryEnterCriticalSection(&cs_))
				return false;
			FlushLocked();
			LeaveCriticalSection(&cs_);
			return true;
		}
		size_t Pending() const {
			return buf_.size();
		}

	private:
		void FlushLocked() {
			if (buf_.empty())
				return;
			EnsureOpen();
			if (h_ != INVALID_HANDLE_VALUE) {
				DWORD written = 0;
				WriteFile(h_, buf_.data(), (DWORD)buf_.size(), &written, nullptr);
			}
			buf_.clear();   // bez pliku linie przepadaja (jak dawniej przy nieudanym fopen)
		}

		void EnsureOpen() {
			ULONGLONG now = GetTickCount64();
			if (h_ != INVALID_HANDLE_VALUE && now - checked_ >= RECHECK_MS) {
				checked_ = now;
				if (GetFileAttributesA(full_) == INVALID_FILE_ATTRIBUTES) {   // usuniety / przeniesiony / czeka na usuniecie
					CloseHandle(h_);
					h_ = INVALID_HANDLE_VALUE;
				}
			}
			if (h_ != INVALID_HANDLE_VALUE)
				return;
			if (failed_ && now - checked_ < RECHECK_MS)
				return;
			if (!full_[0] && !GetFullPathNameA(path_, MAX_PATH, full_, nullptr))
				lstrcpynA(full_, path_, MAX_PATH);   // sciezka wzgledem katalogu biezacego przy pierwszym zapisie (katalog gry)
			h_ = CreateFileA(full_, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
				OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			failed_ = h_ == INVALID_HANDLE_VALUE;
			checked_ = now;
		}

		void Register();
		void Unregister();

		CRITICAL_SECTION cs_;
		char path_[MAX_PATH] = {};
		char full_[MAX_PATH] = {};
		HANDLE h_ = INVALID_HANDLE_VALUE;
		ULONGLONG checked_ = 0;
		bool failed_ = false;
		bool registered_ = false;
		std::string buf_;
	};

	// Tylko bledy krytyczne (0xC.......: naruszenie dostepu, przepelnienie stosu, ...), nie wyjatki C++ ani komunikaty
	// debuggera. Zawsze dalej do kolejnych handlerow - tylko zapis buforow.
	inline LONG CALLBACK CrashFlush(EXCEPTION_POINTERS* ep) {
		DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
		if ((code & 0xF0000000u) == 0xC0000000u)
			for (int i = 0; i < MAX_FILES; i++)
				if (LogFile* f = g_files[i])
					f->TryFlush();
		return EXCEPTION_CONTINUE_SEARCH;
	}

	inline void LogFile::Register() {
		registered_ = true;
		for (int i = 0; i < MAX_FILES; i++)
			if (!g_files[i]) {
				g_files[i] = this;
				break;
			}
		if (!g_handler)
			g_handler = AddVectoredExceptionHandler(0, CrashFlush);
	}

	inline void LogFile::Unregister() {
		bool any = false;
		for (int i = 0; i < MAX_FILES; i++) {
			if (g_files[i] == this)
				g_files[i] = nullptr;
			any = any || g_files[i];
		}
		if (!any && g_handler) {
			RemoveVectoredExceptionHandler(g_handler);
			g_handler = nullptr;
		}
	}
}
