// Disable all warnings from gcc/clang:
#if defined(__clang__)
	#pragma clang system_header
#elif defined(__GNUC__)
	#pragma GCC system_header
#endif

#if defined(_MSC_VER)
#include <sal.h>	// Needed for _In_z_ etc annotations
#endif

#if defined(__linux__) || defined(__APPLE__)
#define LOG_SYSLOG 1
#else
#define LOG_SYSLOG 0
#endif

// ----------------------------------------------------------------------------

#ifndef LOG_SCOPE_TEXT_SIZE
	// Maximum length of text that can be printed by a LOG_SCOPE.
	// This should be long enough to get most things, but short enough not to clutter the stack.
	#define LOG_SCOPE_TEXT_SIZE 196
#endif

#ifndef LOG_REDEFINE_ASSERT
	#define LOG_REDEFINE_ASSERT 0
#endif

#ifndef LOG_WITH_STREAMS
	#define LOG_WITH_STREAMS 1
#endif

#ifndef LOG_WITH_FILEABS
	#define LOG_WITH_FILEABS 0
#endif

// --------------------------------------------------------------------
// Utility macros

#define LOG_CONCATENATE(s1, s2) s1 ## s2

#ifdef __COUNTER__
#   define LOG_ANONYMOUS_VARIABLE(str) LOG_CONCATENATE(str, __COUNTER__)
#else
#   define LOG_ANONYMOUS_VARIABLE(str) LOG_CONCATENATE(str, __LINE__)
#endif

#if defined(__clang__) || defined(__GNUC__)
	// Helper macro for declaring functions as having similar signature to printf.
	// This allows the compiler to catch format errors at compile-time.
	#define LOG_PRINTF_LIKE(fmtarg, firstvararg) __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
	#define LOG_FORMAT_STRING_TYPE const char*
#elif defined(_MSC_VER)
	#define LOG_PRINTF_LIKE(fmtarg, firstvararg)
	#define LOG_FORMAT_STRING_TYPE _In_z_ _Printf_format_string_ const char*
#else
	#define LOG_PRINTF_LIKE(fmtarg, firstvararg)
	#define LOG_FORMAT_STRING_TYPE const char*
#endif

// Used to mark log_and_abort for the benefit of the static analyzer and optimizer.
#if defined(_MSC_VER)
#define LOG_NORETURN __declspec(noreturn)
#else
#define LOG_NORETURN __attribute__((noreturn))
#endif

#if defined(_MSC_VER)
#define LOG_PREDICT_FALSE(x) (x)
#define LOG_PREDICT_TRUE(x)  (x)
#else
#define LOG_PREDICT_FALSE(x) (__builtin_expect(x,     0))
#define LOG_PREDICT_TRUE(x)  (__builtin_expect(!!(x), 1))
#endif

#ifdef _WIN32
	#define STRDUP(str) _strdup(str)
#else
	#define STRDUP(str) strdup(str)
#endif

#include <stdarg.h>

// --------------------------------------------------------------------

namespace aer::log
{
	// Simple RAII ownership of a char*.
	class Text
	{
	public:
		explicit Text(char* owned_str) : _str(owned_str) {}
		~Text();
		Text(Text&& t)
		{
			_str = t._str;
			t._str = nullptr;
		}
		Text(Text& t) = delete;
		Text& operator=(Text& t) = delete;
		void operator=(Text&& t) = delete;

		const char* c_str() const { return _str; }
		bool empty() const { return _str == nullptr || *_str == '\0'; }

		char* release()
		{
			auto result = _str;
			_str = nullptr;
			return result;
		}

	private:
		char* _str;
	};

	// Like printf, but returns the formated text.
	Text textprintf(LOG_FORMAT_STRING_TYPE format, ...) LOG_PRINTF_LIKE(1, 2);

	// Overloaded for variadic template matching.
	
	Text textprintf();

	using Verbosity = int;

#undef FATAL
#undef ERROR
#undef WARNING
#undef INFO
#undef MAX

	enum NamedVerbosity : Verbosity
	{
		// Used to mark an invalid verbosity. Do not log to this level.
		Verbosity_INVALID = -10, // Never do LOG_F(INVALID)

		// You may use Verbosity_OFF on g_stderr_verbosity, but for nothing else!
		Verbosity_OFF     = -9, // Never do LOG_F(OFF)

		// Prefer to use ABORT_F or ABORT_S over LOG_F(FATAL) or LOG_S(FATAL).
		Verbosity_FATAL   = -3,
		Verbosity_ERROR   = -2,
		Verbosity_WARNING = -1,

		// Normal messages. By default written to stderr.
		Verbosity_INFO    =  0,

		// Same as Verbosity_INFO in every way.
		Verbosity_0       =  0,

		// Verbosity levels 1-9 are generally not written to stderr, but are written to file.
		Verbosity_1       = +1,
		Verbosity_2       = +2,
		Verbosity_3       = +3,
		Verbosity_4       = +4,
		Verbosity_5       = +5,
		Verbosity_6       = +6,
		Verbosity_7       = +7,
		Verbosity_8       = +8,
		Verbosity_9       = +9,

		// Do not use higher verbosity levels, as that will make grepping log files harder.
		Verbosity_MAX     = +9,
	};

	struct Message
	{
		// You would generally print a Message by just concatenating the buffers without spacing.
		// Optionally, ignore preamble and indentation.
		Verbosity   verbosity;   // Already part of preamble
		const char* filename;    // Already part of preamble
		unsigned    line;        // Already part of preamble
		const char* preamble;    // Date, time, uptime, thread, file:line, verbosity.
		const char* indentation; // Just a bunch of spacing.
		const char* prefix;      // Assertion failure info goes here (or "").
		const char* message;     // User message goes here.
	};

	/* Everything with a verbosity equal or greater than g_stderr_verbosity will be
	written to stderr. You can set this in code or via the -v argument.
	Set to aer::log::Verbosity_OFF to write nothing to stderr.
	Default is 0, i.e. only log ERROR, WARNING and INFO are written to stderr.
	*/
	 extern Verbosity g_stderr_verbosity;
	 extern bool      g_colorlogtostderr; // True by default.
	 extern unsigned  g_flush_interval_ms; // 0 (unbuffered) by default.
	 extern bool      g_preamble_header; // Prepend each log start by a descriptions line with all columns name? True by default.
	 extern bool      g_preamble; // Prefix each log line with date, time etc? True by default.

	/* Specify the verbosity used to log its info messages including the header
	logged when logged::init() is called or on exit. Default is 0 (INFO).
	*/
	 extern Verbosity g_internal_verbosity;

	// Turn off individual parts of the preamble
	 extern bool      g_preamble_date; // The date field
	 extern bool      g_preamble_time; // The time of the current day
	 extern bool      g_preamble_uptime; // The time since init call
	 extern bool      g_preamble_thread; // The logging thread
	 extern bool      g_preamble_file; // The file from which the log originates from
	 extern bool      g_preamble_verbose; // The verbosity field
	 extern bool      g_preamble_pipe; // The pipe symbol right before the message

	// May not throw!
	typedef void (*log_handler_t)(void* user_data, const Message& message);
	typedef void (*close_handler_t)(void* user_data);
	typedef void (*flush_handler_t)(void* user_data);

	// May throw if that's how you'd like to handle your errors.
	typedef void (*fatal_handler_t)(const Message& message);

	// Given a verbosity level, return the level's name or nullptr.
	typedef const char* (*verbosity_to_name_t)(Verbosity verbosity);

	// Given a verbosity level name, return the verbosity level or
	// Verbosity_INVALID if name is not recognized.
	typedef Verbosity (*name_to_verbosity_t)(const char* name);

	struct SignalOptions
	{
		/// Make logger try to do unsafe but useful things,
		/// like printing a stack trace, when catching signals.
		/// This may lead to bad things like deadlocks in certain situations.
		bool unsafe_signal_handler = true;

		/// Should logger catch SIGABRT ?
		bool sigabrt = true;

		/// Should logger catch SIGBUS ?
		bool sigbus = true;

		/// Should logger catch SIGFPE ?
		bool sigfpe = true;

		/// Should logger catch SIGILL ?
		bool sigill = true;

		/// Should logger catch SIGINT ?
		bool sigint = true;

		/// Should logger catch SIGSEGV ?
		bool sigsegv = true;

		/// Should logger catch SIGTERM ?
		bool sigterm = true;

		static SignalOptions none()
		{
			SignalOptions options;
			options.unsafe_signal_handler = false;
			options.sigabrt = false;
			options.sigbus = false;
			options.sigfpe = false;
			options.sigill = false;
			options.sigint = false;
			options.sigsegv = false;
			options.sigterm = false;
			return options;
		}
	};

	// Runtime options passed to aer::log::init
	struct Options
	{
		// This allows you to use something else instead of "-v" via verbosity_flag.
		// Set to nullptr if you don't want logger to parse verbosity from the args.
		const char* verbosity_flag = "-v";

		// aer::log::init will set the name of the calling thread to this.
		// If you don't want logger to set the name of the main thread,
		// set this to nullptr.
		// NOTE: on SOME platforms aer::log::init will only overwrite the thread name
		// if a thread name has not already been set.
		// To always set a thread name, use aer::log::set_thread_name instead.
		const char* main_thread_name = "main thread";

		SignalOptions signal_options;
	};

	/*  Should be called from the main thread.
		You don't *need* to call this, but if you do you get:
			* Signal handlers installed
			* Program arguments logged
			* Working dir logged
			* Optional -v verbosity flag parsed
			* Main thread name set to "main thread"
			* Explanation of the preamble (date, thread name, etc) logged

		aer::log::init() will look for arguments meant for the logger and remove them.
		Arguments meant for the logger are:
			-v n   Set aer::log::g_stderr_verbosity level. Examples:
				-v 3        Show verbosity level 3 and lower.
				-v 0        Only show INFO, WARNING, ERROR, FATAL (default).
				-v INFO     Only show INFO, WARNING, ERROR, FATAL (default).
				-v WARNING  Only show WARNING, ERROR, FATAL.
				-v ERROR    Only show ERROR, FATAL.
				-v FATAL    Only show FATAL.
				-v OFF      Turn off logging to stderr.

		Tip: You can set g_stderr_verbosity before calling aer::log::init.
		That way you can set the default but have the user override it with the -v flag.
		Note that -v does not affect file logging (see aer::log::add_file).

		You can you something other than the -v flag by setting the verbosity_flag option.
	*/
	
	void init(int& argc, char* argv[], const Options& options = {});

	// Will call remove_all_callbacks(). After calling this, logging will still go to stderr.
	// You generally don't need to call this.
	
	void shutdown();

	// What ~ will be replaced with, e.g. "/home/your_user_name/"
	
	const char* home_dir();

	/* Returns the name of the app as given in argv[0] but without leading path.
	   That is, if argv[0] is "../foo/app" this will return "app".
	*/
	
	const char* argv0_filename();

	// Returns all arguments given to aer::log::init(), but escaped with a single space as separator.
	
	const char* arguments();

	// Returns the path to the current working dir when aer::log::init() was called.
	
	const char* current_dir();

	// Returns the part of the path after the last / or \ (if any).
	
	const char* filename(const char* path);

	// e.g. "foo/bar/baz.ext" will create the directories "foo/" and "foo/bar/"
	
	bool create_directories(const char* file_path_const);

	// Writes date and time with millisecond precision, e.g. "20151017_161503.123"
	
	void write_date_time(char* buff, unsigned long long buff_size);

	// Helper: thread-safe version strerror
	
	Text errno_as_text();

	/* Given a prefix of e.g. "~/aer/" this might return
	   "/home/your_username/aer/app_name/20151017_161503.123.log"

	   where "app_name" is a sanitized version of argv[0].
	*/
	
	void suggest_log_path(const char* prefix, char* buff, unsigned long long buff_size);

	enum FileMode { Truncate, Append };

	/*  Will log to a file at the given path.
		Any logging message with a verbosity lower or equal to
		the given verbosity will be included.
		The function will create all directories in 'path' if needed.
		If path starts with a ~, it will be replaced with aer::log::home_dir()
		To stop the file logging, just call aer::log::remove_callback(path) with the same path.
	*/
	
	bool add_file(const char* path, FileMode mode, Verbosity verbosity);

	
	// Send logs to syslog with LOG_USER facility (see next call)
	bool add_syslog(const char* app_name, Verbosity verbosity);
	
	// Send logs to syslog with your own choice of facility (LOG_USER, LOG_AUTH, ...)
	// see log.cpp: syslog_log() for more details.
	bool add_syslog(const char* app_name, Verbosity verbosity, int facility);

	/*  Will be called right before abort().
		You can for instance use this to print custom error messages, or throw an exception.
		Feel free to call LOG:ing function from this, but not FATAL ones! */
	
	void set_fatal_handler(fatal_handler_t handler);

	// Get the current fatal handler, if any. Default value is nullptr.
	
	fatal_handler_t get_fatal_handler();

	/*  Will be called on each log messages with a verbosity less or equal to the given one.
		Useful for displaying messages on-screen in a game, for example.
		The given on_close is also expected to flush (if desired).
	*/
	
	void add_callback(
		const char*     id,
		log_handler_t   callback,
		void*           user_data,
		Verbosity       verbosity,
		close_handler_t on_close = nullptr,
		flush_handler_t on_flush = nullptr);

	/*  Set a callback that returns custom verbosity level names. If callback
		is nullptr or returns nullptr, default log names will be used.
	*/
	
	void set_verbosity_to_name_callback(verbosity_to_name_t callback);

	/*  Set a callback that returns the verbosity level matching a name. The
		callback should return Verbosity_INVALID if the name is not
		recognized.
	*/
	
	void set_name_to_verbosity_callback(name_to_verbosity_t callback);

	/*  Get a custom name for a specific verbosity, if one exists, or nullptr. */
	
	const char* get_verbosity_name(Verbosity verbosity);

	/*  Get the verbosity enum value from a custom 4-character level name, if one exists.
		If the name does not match a custom level name, Verbosity_INVALID is returned.
	*/
	
	Verbosity get_verbosity_from_name(const char* name);

	// Returns true iff the callback was found (and removed).
	
	bool remove_callback(const char* id);

	// Shut down all file logging and any other callback hooks installed.
	
	void remove_all_callbacks();

	// Returns the maximum of g_stderr_verbosity and all file/custom outputs.
	
	Verbosity current_verbosity_cutoff();

	// Actual logging function. Use the LOG macro instead of calling this directly.
	
	void log(Verbosity verbosity, const char* file, unsigned line, LOG_FORMAT_STRING_TYPE format, ...) LOG_PRINTF_LIKE(4, 5);

	// Actual logging function.
	
	void vlog(Verbosity verbosity, const char* file, unsigned line, LOG_FORMAT_STRING_TYPE format, va_list) LOG_PRINTF_LIKE(4, 0);

	// Log without any preamble or indentation.
	
	void raw_log(Verbosity verbosity, const char* file, unsigned line, LOG_FORMAT_STRING_TYPE format, ...) LOG_PRINTF_LIKE(4, 5);

	// Helper class for LOG_SCOPE_F
	class LogScopeRAII
	{
	public:
		LogScopeRAII() : _file(nullptr) {} // No logging
		LogScopeRAII(Verbosity verbosity, const char* file, unsigned line, LOG_FORMAT_STRING_TYPE format, va_list vlist) LOG_PRINTF_LIKE(5, 0);
		LogScopeRAII(Verbosity verbosity, const char* file, unsigned line, LOG_FORMAT_STRING_TYPE format, ...) LOG_PRINTF_LIKE(5, 6);
		~LogScopeRAII();

		void Init(LOG_FORMAT_STRING_TYPE format, va_list vlist) LOG_PRINTF_LIKE(2, 0);

#if defined(_MSC_VER) && _MSC_VER > 1800
		// older MSVC default move ctors close the scope on move. See
		// issue #43
		LogScopeRAII(LogScopeRAII&& other)
			: _verbosity(other._verbosity)
			, _file(other._file)
			, _line(other._line)
			, _indent_stderr(other._indent_stderr)
			, _start_time_ns(other._start_time_ns)
		{
			// Make sure the tmp object's destruction doesn't close the scope:
			other._file = nullptr;

			for (unsigned int i = 0; i < LOG_SCOPE_TEXT_SIZE; ++i) {
				_name[i] = other._name[i];
			}
		}
#else
		LogScopeRAII(LogScopeRAII&&) = default;
#endif

	private:
		LogScopeRAII(const LogScopeRAII&) = delete;
		LogScopeRAII& operator=(const LogScopeRAII&) = delete;
		void operator=(LogScopeRAII&&) = delete;

		Verbosity   _verbosity;
		const char* _file; // Set to null if we are disabled due to verbosity
		unsigned    _line;
		bool        _indent_stderr; // Did we?
		long long   _start_time_ns;
		char        _name[LOG_SCOPE_TEXT_SIZE];
	};

	// Marked as 'noreturn' for the benefit of the static analyzer and optimizer.
	// stack_trace_skip is the number of extrace stack frames to skip above log_and_abort.
	LOG_NORETURN void log_and_abort(int stack_trace_skip, const char* expr, const char* file, unsigned line, LOG_FORMAT_STRING_TYPE format, ...) LOG_PRINTF_LIKE(5, 6);
	
	LOG_NORETURN void log_and_abort(int stack_trace_skip, const char* expr, const char* file, unsigned line);

	// Flush output to stderr and files.
	// If g_flush_interval_ms is set to non-zero, this will be called automatically this often.
	// If not set, you do not need to call this at all.
	
	void flush();

	template<class T> inline Text format_value(const T&)                    { return textprintf("N/A");    }
	template<>        inline Text format_value(const char& v)               { return textprintf("%",   v); }
	template<>        inline Text format_value(const int& v)                { return textprintf("%",   v); }
	template<>        inline Text format_value(const float& v)              { return textprintf("%",   v); }
	template<>        inline Text format_value(const double& v)             { return textprintf("%",   v); }

	template<>        inline Text format_value(const unsigned int& v)       { return textprintf("%",   v); }
	template<>        inline Text format_value(const long& v)               { return textprintf("%l",  v); }
	template<>        inline Text format_value(const unsigned long& v)      { return textprintf("%l",  v); }
	template<>        inline Text format_value(const long long& v)          { return textprintf("%ll", v); }
	template<>        inline Text format_value(const unsigned long long& v) { return textprintf("%ll", v); }

	/* Thread names can be set for the benefit of readable logs.
	   If you do not set the thread name, a hex id will be shown instead.
	   These thread names may or may not be the same as the system thread names,
	   depending on the system.
	   Try to limit the thread name to 15 characters or less. */
	
	void set_thread_name(const char* name);

	/* Returns the thread name for this thread.
	   On most *nix systems this will return the system thread name (settable from both within and without logger).
	   On other systems it will return whatever you set in `set_thread_name()`;
	   If no thread name is set, this will return a hexadecimal thread id.
	   `length` should be the number of bytes available in the buffer.
	   17 is a good number for length.
	   `right_align_hex_id` means any hexadecimal thread id will be written to the end of buffer.
	*/
	
	void get_thread_name(char* buffer, unsigned long long length, bool right_align_hex_id);

	/* Generates a readable stacktrace as a string.
	   'skip' specifies how many stack frames to skip.
	   For instance, the default skip (1) means:
	   don't include the call to aer::log::stacktrace in the stack trace. */
	
	Text stacktrace(int skip = 1);

	/*  Add a string to be replaced with something else in the stack output.

		For instance, instead of having a stack trace look like this:
			0x41f541 some_function(std::basic_ofstream<char, std::char_traits<char> >&)
		You can clean it up with:
			auto verbose_type_name = aer::log::demangle(typeid(std::ofstream).name());
			aer::log::add_stack_cleanup(verbose_type_name.c_str(); "std::ofstream");
		So the next time you will instead see:
			0x41f541 some_function(std::ofstream&)

		`replace_with_this` must be shorter than `find_this`.
	*/
	
	void add_stack_cleanup(const char* find_this, const char* replace_with_this);

	// Example: demangle(typeid(std::ofstream).name()) -> "std::basic_ofstream<char, std::char_traits<char> >"
	
	Text demangle(const char* name);

	// ------------------------------------------------------------------------
	/*
	Not all terminals support colors, but if they do, and g_colorlogtostderr
	is set, logger will write them to stderr to make errors in red, etc.

	You also have the option to manually use them, via the function below.

	Note, however, that if you do, the color codes could end up in your logfile!

	This means if you intend to use them functions you should either:
		* Use them on the stderr/stdout directly (bypass logger).
		* Don't add file outputs to logger.
		* Expect some \e[1m things in your logfile.

	Usage:
		printf("%sRed%sGreen%sBold green%sClear again\n",
			   aer::log::terminal_red(), aer::log::terminal_green(),
			   aer::log::terminal_bold(), aer::log::terminal_reset());

	If the terminal at hand does not support colors the above output
	will just not have funky \e[1m things showing.
	*/

	// Do the output terminal support colors?
	
	bool terminal_has_color();

	// Colors
	 const char* terminal_black();
	 const char* terminal_red();
	 const char* terminal_green();
	 const char* terminal_yellow();
	 const char* terminal_blue();
	 const char* terminal_purple();
	 const char* terminal_cyan();
	 const char* terminal_light_gray();
	 const char* terminal_light_red();
	 const char* terminal_white();

	// Formating
	 const char* terminal_bold();
	 const char* terminal_underline();

	// You should end each line with this!
	 const char* terminal_reset();

	// --------------------------------------------------------------------
	// Error context related:

	struct StringStream;

	// Use this in your EcEntryBase::print_value overload.
	
	void stream_print(StringStream& out_string_stream, const char* text);

	class EcEntryBase
	{
	public:
		EcEntryBase(const char* file, unsigned line, const char* descr);
		~EcEntryBase();
		EcEntryBase(const EcEntryBase&) = delete;
		EcEntryBase(EcEntryBase&&) = delete;
		EcEntryBase& operator=(const EcEntryBase&) = delete;
		EcEntryBase& operator=(EcEntryBase&&) = delete;

		virtual void print_value(StringStream& out_string_stream) const = 0;

		EcEntryBase* previous() const { return _previous; }

	// private:
		const char*  _file;
		unsigned     _line;
		const char*  _descr;
		EcEntryBase* _previous;
	};

	template<typename T>
	class EcEntryData : public EcEntryBase
	{
	public:
		using Printer = Text(*)(T data);

		EcEntryData(const char* file, unsigned line, const char* descr, T data, Printer&& printer)
			: EcEntryBase(file, line, descr), _data(data), _printer(printer) {}

		virtual void print_value(StringStream& out_string_stream) const override
		{
			const auto str = _printer(_data);
			stream_print(out_string_stream, str.c_str());
		}

	private:
		T       _data;
		Printer _printer;
	};

	// template<typename Printer>
	// class EcEntryLambda : public EcEntryBase
	// {
	// public:
	// 	EcEntryLambda(const char* file, unsigned line, const char* descr, Printer&& printer)
	// 		: EcEntryBase(file, line, descr), _printer(std::move(printer)) {}

	// 	virtual void print_value(StringStream& out_string_stream) const override
	// 	{
	// 		const auto str = _printer();
	// 		stream_print(out_string_stream, str.c_str());
	// 	}

	// private:
	// 	Printer _printer;
	// };

	// template<typename Printer>
	// EcEntryLambda<Printer> make_ec_entry_lambda(const char* file, unsigned line, const char* descr, Printer&& printer)
	// {
	// 	return {file, line, descr, std::move(printer)};
	// }

	template <class T>
	struct decay_char_array { using type = T; };

	template <unsigned long long  N>
	struct decay_char_array<const char(&)[N]> { using type = const char*; };

	template <class T>
	struct make_const_ptr { using type = T; };

	template <class T>
	struct make_const_ptr<T*> { using type = const T*; };

	template <class T>
	struct make_ec_type { using type = typename make_const_ptr<typename decay_char_array<T>::type>::type; };

	/* 	A stack trace gives you the names of the function at the point of a crash.
		With ERROR_CONTEXT, you can also get the values of select local variables.
		Usage:

		void process_customers(const std::string& filename)
		{
			ERROR_CONTEXT("Processing file", filename.c_str());
			for (int customer_index : ...)
			{
				ERROR_CONTEXT("Customer index", customer_index);
				...
			}
		}

		The context is in effect during the scope of the ERROR_CONTEXT.
		Use aer::log::get_error_context() to get the contents of the active error contexts.

		Example result:

		------------------------------------------------
		[ErrorContext]                main.cpp:416   Processing file:    "customers.json"
		[ErrorContext]                main.cpp:417   Customer index:     42
		------------------------------------------------

		Error contexts are printed automatically on crashes, and only on crashes.
		This makes them much faster than logging the value of a variable.
	*/
	#define ERROR_CONTEXT(descr, data)                                             \
		const aer::log::EcEntryData<aer::log::make_ec_type<decltype(data)>::type>      \
			LOG_ANONYMOUS_VARIABLE(error_context_scope_)(                       \
				__FILE__, __LINE__, descr, data,                                   \
				static_cast<aer::log::EcEntryData<aer::log::make_ec_type<decltype(data)>::type>::Printer>(aer::log::ec_to_text) ) // For better error messages

/*
	#define ERROR_CONTEXT(descr, data)                                 \
		const auto LOG_ANONYMOUS_VARIABLE(error_context_scope_)(    \
			aer::log::make_ec_entry_lambda(__FILE__, __LINE__, descr,    \
				[=](){ return aer::log::ec_to_text(data); }))
*/

	using EcHandle = const EcEntryBase*;

	/*
		Get a light-weight handle to the error context stack on this thread.
		The handle is valid as long as the current thread has no changes to its error context stack.
		You can pass the handle to aer::log::get_error_context on another thread.
		This can be very useful for when you have a parent thread spawning several working threads,
		and you want the error context of the parent thread to get printed (too) when there is an
		error on the child thread. You can accomplish this thusly:

		void foo(const char* parameter)
		{
			ERROR_CONTEXT("parameter", parameter)
			const auto parent_ec_handle = aer::log::get_thread_ec_handle();

			std::thread([=]{
				aer::log::set_thread_name("child thread");
				ERROR_CONTEXT("parent context", parent_ec_handle);
				dangerous_code();
			}.join();
		}

	*/
	
	EcHandle get_thread_ec_handle();

	// Get a string describing the current stack of error context. Empty string if there is none.
	
	Text get_error_context();

	// Get a string describing the error context of the given thread handle.
	
	Text get_error_context_for(EcHandle ec_handle);

	// ------------------------------------------------------------------------

	 Text ec_to_text(const char* data);
	 Text ec_to_text(char data);
	 Text ec_to_text(int data);
	 Text ec_to_text(unsigned int data);
	 Text ec_to_text(long data);
	 Text ec_to_text(unsigned long data);
	 Text ec_to_text(long long data);
	 Text ec_to_text(unsigned long long data);
	 Text ec_to_text(float data);
	 Text ec_to_text(double data);
	 Text ec_to_text(long double data);
	 Text ec_to_text(EcHandle);

	/*
	You can add ERROR_CONTEXT support for your own types by overloading ec_to_text. Here's how:

	some.hpp:
		namespace aer::log {
			Text ec_to_text(MySmallType data)
			Text ec_to_text(const MyBigType* data)
		} // namespace aer::log

	some.cpp:
		namespace aer::log {
			Text ec_to_text(MySmallType small_value)
			{
				// Called only when needed, i.e. on a crash.
				std::string str = small_value.as_string(); // Format 'small_value' here somehow.
				return Text{STRDUP(str.c_str())};
			}

			Text ec_to_text(const MyBigType* big_value)
			{
				// Called only when needed, i.e. on a crash.
				std::string str = big_value->as_string(); // Format 'big_value' here somehow.
				return Text{STRDUP(str.c_str())};
			}
		} // namespace aer::log

	Any file that include some.hpp:
		void foo(MySmallType small, const MyBigType& big)
		{
			ERROR_CONTEXT("Small", small); // Copy ´small` by value.
			ERROR_CONTEXT("Big",   &big);  // `big` should not change during this scope!
			....
		}
	*/
} // namespace aer::log

// --------------------------------------------------------------------
// Logging macros

// LOG_F(2, "Only logged if verbosity is 2 or higher: %d", some_number);
#define VLOG_F(verbosity, ...)                                                                     \
	((verbosity) > aer::log::current_verbosity_cutoff()) ? (void)0                                   \
									  : aer::log::log(verbosity, __FILE__, __LINE__, __VA_ARGS__)

// LOG_F(INFO, "Foo: %d", some_number);
#define LOG_F(verbosity_name, ...) VLOG_F(aer::log::Verbosity_ ## verbosity_name, __VA_ARGS__)

#define VLOG_IF_F(verbosity, cond, ...)                                                            \
	((verbosity) > aer::log::current_verbosity_cutoff() || (cond) == false)                          \
		? (void)0                                                                                  \
		: aer::log::log(verbosity, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_IF_F(verbosity_name, cond, ...)                                                        \
	VLOG_IF_F(aer::log::Verbosity_ ## verbosity_name, cond, __VA_ARGS__)

#define VLOG_SCOPE_F(verbosity, ...)                                                               \
	aer::log::LogScopeRAII LOG_ANONYMOUS_VARIABLE(error_context_RAII_) =                          \
	((verbosity) > aer::log::current_verbosity_cutoff()) ? aer::log::LogScopeRAII() :                  \
	aer::log::LogScopeRAII(verbosity, __FILE__, __LINE__, __VA_ARGS__)

// Raw logging - no preamble, no indentation. Slightly faster than full logging.
#define RAW_VLOG_F(verbosity, ...)                                                                 \
	((verbosity) > aer::log::current_verbosity_cutoff()) ? (void)0                                   \
									  : aer::log::raw_log(verbosity, __FILE__, __LINE__, __VA_ARGS__)

#define RAW_LOG_F(verbosity_name, ...) RAW_VLOG_F(aer::log::Verbosity_ ## verbosity_name, __VA_ARGS__)

// Use to book-end a scope. Affects logging on all threads.
#define LOG_SCOPE_F(verbosity_name, ...)                                                           \
	VLOG_SCOPE_F(aer::log::Verbosity_ ## verbosity_name, __VA_ARGS__)

#define LOG_SCOPE_FUNCTION(verbosity_name) LOG_SCOPE_F(verbosity_name, __func__)

// -----------------------------------------------
// ABORT_F macro. Usage:  ABORT_F("Cause of error: %s", error_str);

// Message is optional
#define ABORT_F(...) aer::log::log_and_abort(0, "ABORT: ", __FILE__, __LINE__, __VA_ARGS__)

// --------------------------------------------------------------------
// CHECK_F macros:

#define CHECK_WITH_INFO_F(test, info, ...)                                                         \
	LOG_PREDICT_TRUE((test) == true) ? (void)0 : aer::log::log_and_abort(0, "CHECK FAILED:  " info "  ", __FILE__,      \
													   __LINE__, ##__VA_ARGS__)

/* Checked at runtime too. Will print error, then call fatal_handler (if any), then 'abort'.
   Note that the test must be boolean.
   CHECK_F(ptr); will not compile, but CHECK_F(ptr != nullptr); will. */
#define CHECK_F(test, ...) CHECK_WITH_INFO_F(test, #test, ##__VA_ARGS__)

#define CHECK_NOTNULL_F(x, ...) CHECK_WITH_INFO_F((x) != nullptr, #x " != nullptr", ##__VA_ARGS__)

#define CHECK_OP_F(expr_left, expr_right, op, ...)                                                 \
	do                                                                                             \
	{                                                                                              \
		auto val_left = expr_left;                                                                 \
		auto val_right = expr_right;                                                               \
		if (! LOG_PREDICT_TRUE(val_left op val_right))                                             \
		{                                                                                          \
			auto str_left = aer::log::format_value(val_left);                                        \
			auto str_right = aer::log::format_value(val_right);                                      \
			auto fail_info = aer::log::textprintf("CHECK FAILED:  %s %s %s  (%s %s %s)  ",           \
				#expr_left, #op, #expr_right, str_left.c_str(), #op, str_right.c_str());           \
			auto user_msg = aer::log::textprintf(__VA_ARGS__);                                       \
			aer::log::log_and_abort(0, fail_info.c_str(), __FILE__, __LINE__,                        \
			                      "%s", user_msg.c_str());                                           \
		}                                                                                          \
	} while (false)

#ifndef LOG_DEBUG_LOGGING
	#ifndef NDEBUG
		#define LOG_DEBUG_LOGGING 1
	#else
		#define LOG_DEBUG_LOGGING 0
	#endif
#endif

#if LOG_DEBUG_LOGGING
	// Debug logging enabled:
	#define DLOG_F(verbosity_name, ...)     LOG_F(verbosity_name, __VA_ARGS__)
	#define DVLOG_F(verbosity, ...)         VLOG_F(verbosity, __VA_ARGS__)
	#define DLOG_IF_F(verbosity_name, ...)  LOG_IF_F(verbosity_name, __VA_ARGS__)
	#define DVLOG_IF_F(verbosity, ...)      VLOG_IF_F(verbosity, __VA_ARGS__)
	#define DRAW_LOG_F(verbosity_name, ...) RAW_LOG_F(verbosity_name, __VA_ARGS__)
	#define DRAW_VLOG_F(verbosity, ...)     RAW_VLOG_F(verbosity, __VA_ARGS__)
#else
	// Debug logging disabled:
	#define DLOG_F(verbosity_name, ...)
	#define DVLOG_F(verbosity, ...)
	#define DLOG_IF_F(verbosity_name, ...)
	#define DVLOG_IF_F(verbosity, ...)
	#define DRAW_LOG_F(verbosity_name, ...)
	#define DRAW_VLOG_F(verbosity, ...)
#endif

#define CHECK_EQ_F(a, b, ...) CHECK_OP_F(a, b, ==, ##__VA_ARGS__)
#define CHECK_NE_F(a, b, ...) CHECK_OP_F(a, b, !=, ##__VA_ARGS__)
#define CHECK_LT_F(a, b, ...) CHECK_OP_F(a, b, < , ##__VA_ARGS__)
#define CHECK_GT_F(a, b, ...) CHECK_OP_F(a, b, > , ##__VA_ARGS__)
#define CHECK_LE_F(a, b, ...) CHECK_OP_F(a, b, <=, ##__VA_ARGS__)
#define CHECK_GE_F(a, b, ...) CHECK_OP_F(a, b, >=, ##__VA_ARGS__)

#ifndef LOG_DEBUG_CHECKS
	#ifndef NDEBUG
		#define LOG_DEBUG_CHECKS 1
	#else
		#define LOG_DEBUG_CHECKS 0
	#endif
#endif

#if LOG_DEBUG_CHECKS
	// Debug checks enabled:
	#define DCHECK_F(test, ...)             CHECK_F(test, ##__VA_ARGS__)
	#define DCHECK_NOTNULL_F(x, ...)        CHECK_NOTNULL_F(x, ##__VA_ARGS__)
	#define DCHECK_EQ_F(a, b, ...)          CHECK_EQ_F(a, b, ##__VA_ARGS__)
	#define DCHECK_NE_F(a, b, ...)          CHECK_NE_F(a, b, ##__VA_ARGS__)
	#define DCHECK_LT_F(a, b, ...)          CHECK_LT_F(a, b, ##__VA_ARGS__)
	#define DCHECK_LE_F(a, b, ...)          CHECK_LE_F(a, b, ##__VA_ARGS__)
	#define DCHECK_GT_F(a, b, ...)          CHECK_GT_F(a, b, ##__VA_ARGS__)
	#define DCHECK_GE_F(a, b, ...)          CHECK_GE_F(a, b, ##__VA_ARGS__)
#else
	// Debug checks disabled:
	#define DCHECK_F(test, ...)
	#define DCHECK_NOTNULL_F(x, ...)
	#define DCHECK_EQ_F(a, b, ...)
	#define DCHECK_NE_F(a, b, ...)
	#define DCHECK_LT_F(a, b, ...)
	#define DCHECK_LE_F(a, b, ...)
	#define DCHECK_GT_F(a, b, ...)
	#define DCHECK_GE_F(a, b, ...)
#endif // NDEBUG


#if LOG_REDEFINE_ASSERT
	#undef assert
	#ifndef NDEBUG
		// Debug:
		#define assert(test) CHECK_WITH_INFO_F(!!(test), #test) // HACK
	#else
		#define assert(test)
	#endif
#endif // LOG_REDEFINE_ASSERT

// ----------------------------------------------------------------------------
// .dP"Y8 888888 88""Yb 888888    db    8b    d8 .dP"Y8
// `Ybo."   88   88__dP 88__     dPYb   88b  d88 `Ybo."
// o.`Y8b   88   88"Yb  88""    dP__Yb  88YbdP88 o.`Y8b
// 8bodP'   88   88  Yb 888888 dP""""Yb 88 YY 88 8bodP'

#if LOG_WITH_STREAMS

#include <cstdarg>
#include <sstream> // Adds about 38 kLoC on clang.
#include <string>

namespace aer::log
{
	// Like sprintf, but returns the formated text.
	
	std::string strprintf(LOG_FORMAT_STRING_TYPE format, ...) LOG_PRINTF_LIKE(1, 2);

	// Like vsprintf, but returns the formated text.
	
	std::string vstrprintf(LOG_FORMAT_STRING_TYPE format, va_list) LOG_PRINTF_LIKE(1, 0);

	class StreamLogger
	{
	public:
		StreamLogger(Verbosity verbosity, const char* file, unsigned line) : _verbosity(verbosity), _file(file), _line(line) {}
		~StreamLogger() noexcept(false);

		template<typename T>
		StreamLogger& operator<<(const T& t)
		{
			_ss << t;
			return *this;
		}

		// std::endl and other iomanip:s.
		StreamLogger& operator<<(std::ostream&(*f)(std::ostream&))
		{
			f(_ss);
			return *this;
		}

	private:
		Verbosity   _verbosity;
		const char* _file;
		unsigned    _line;
		std::ostringstream _ss;
	};

	class AbortLogger
	{
	public:
		AbortLogger(const char* expr, const char* file, unsigned line) : _expr(expr), _file(file), _line(line) { }
		LOG_NORETURN ~AbortLogger() noexcept(false);

		template<typename T>
		AbortLogger& operator<<(const T& t)
		{
			_ss << t;
			return *this;
		}

		// std::endl and other iomanip:s.
		AbortLogger& operator<<(std::ostream&(*f)(std::ostream&))
		{
			f(_ss);
			return *this;
		}

	private:
		const char*        _expr;
		const char*        _file;
		unsigned           _line;
		std::ostringstream _ss;
	};

	class Voidify
	{
	public:
		Voidify() {}
		// This has to be an operator with a precedence lower than << but higher than ?:
		void operator&(const StreamLogger&) { }
		void operator&(const AbortLogger&)  { }
	};

	/*  Helper functions for CHECK_OP_S macro.
		GLOG trick: The (int, int) specialization works around the issue that the compiler
		will not instantiate the template version of the function on values of unnamed enum type. */
	#define DEFINE_CHECK_OP_IMPL(name, op)                                                             \
		template <typename T1, typename T2>                                                            \
		inline std::string* name(const char* expr, const T1& v1, const char* op_str, const T2& v2)     \
		{                                                                                              \
			if (LOG_PREDICT_TRUE(v1 op v2)) { return NULL; }                                        \
			std::ostringstream ss;                                                                     \
			ss << "CHECK FAILED:  " << expr << "  (" << v1 << " " << op_str << " " << v2 << ")  ";     \
			return new std::string(ss.str());                                                          \
		}                                                                                              \
		inline std::string* name(const char* expr, int v1, const char* op_str, int v2)                 \
		{                                                                                              \
			return name<int, int>(expr, v1, op_str, v2);                                               \
		}

	DEFINE_CHECK_OP_IMPL(check_EQ_impl, ==)
	DEFINE_CHECK_OP_IMPL(check_NE_impl, !=)
	DEFINE_CHECK_OP_IMPL(check_LE_impl, <=)
	DEFINE_CHECK_OP_IMPL(check_LT_impl, < )
	DEFINE_CHECK_OP_IMPL(check_GE_impl, >=)
	DEFINE_CHECK_OP_IMPL(check_GT_impl, > )
	#undef DEFINE_CHECK_OP_IMPL

	/*  GLOG trick: Function is overloaded for integral types to allow static const integrals
		declared in classes and not defined to be used as arguments to CHECK* macros. */
	template <class T>
	inline const T&           referenceable_value(const T&           t) { return t; }
	inline char               referenceable_value(char               t) { return t; }
	inline unsigned char      referenceable_value(unsigned char      t) { return t; }
	inline signed char        referenceable_value(signed char        t) { return t; }
	inline short              referenceable_value(short              t) { return t; }
	inline unsigned short     referenceable_value(unsigned short     t) { return t; }
	inline int                referenceable_value(int                t) { return t; }
	inline unsigned int       referenceable_value(unsigned int       t) { return t; }
	inline long               referenceable_value(long               t) { return t; }
	inline unsigned long      referenceable_value(unsigned long      t) { return t; }
	inline long long          referenceable_value(long long          t) { return t; }
	inline unsigned long long referenceable_value(unsigned long long t) { return t; }
} // namespace aer::log

// -----------------------------------------------
// Logging macros:

// usage:  LOG_STREAM(INFO) << "Foo " << std::setprecision(10) << some_value;
#define VLOG_IF_S(verbosity, cond)                                                                 \
	((verbosity) > aer::log::current_verbosity_cutoff() || (cond) == false)                          \
		? (void)0                                                                                  \
		: aer::log::Voidify() & aer::log::StreamLogger(verbosity, __FILE__, __LINE__)
#define LOG_IF_S(verbosity_name, cond) VLOG_IF_S(aer::log::Verbosity_ ## verbosity_name, cond)
#define VLOG_S(verbosity)              VLOG_IF_S(verbosity, true)
#define LOG_S(verbosity_name)          VLOG_S(aer::log::Verbosity_ ## verbosity_name)

// -----------------------------------------------
// ABORT_S macro. Usage:  ABORT_S() << "Causo of error: " << details;

#define ABORT_S() aer::log::Voidify() & aer::log::AbortLogger("ABORT: ", __FILE__, __LINE__)

// -----------------------------------------------
// CHECK_S macros:

#define CHECK_WITH_INFO_S(cond, info)                                                              \
	LOG_PREDICT_TRUE((cond) == true)                                                            \
		? (void)0                                                                                  \
		: aer::log::Voidify() & aer::log::AbortLogger("CHECK FAILED:  " info "  ", __FILE__, __LINE__)

#define CHECK_S(cond) CHECK_WITH_INFO_S(cond, #cond)
#define CHECK_NOTNULL_S(x) CHECK_WITH_INFO_S((x) != nullptr, #x " != nullptr")

#define CHECK_OP_S(function_name, expr1, op, expr2)                                                \
	while (auto error_string = aer::log::function_name(#expr1 " " #op " " #expr2,                    \
													 aer::log::referenceable_value(expr1), #op,      \
													 aer::log::referenceable_value(expr2)))          \
		aer::log::AbortLogger(error_string->c_str(), __FILE__, __LINE__)

#define CHECK_EQ_S(expr1, expr2) CHECK_OP_S(check_EQ_impl, expr1, ==, expr2)
#define CHECK_NE_S(expr1, expr2) CHECK_OP_S(check_NE_impl, expr1, !=, expr2)
#define CHECK_LE_S(expr1, expr2) CHECK_OP_S(check_LE_impl, expr1, <=, expr2)
#define CHECK_LT_S(expr1, expr2) CHECK_OP_S(check_LT_impl, expr1, < , expr2)
#define CHECK_GE_S(expr1, expr2) CHECK_OP_S(check_GE_impl, expr1, >=, expr2)
#define CHECK_GT_S(expr1, expr2) CHECK_OP_S(check_GT_impl, expr1, > , expr2)

#if LOG_DEBUG_LOGGING
	// Debug logging enabled:
	#define DVLOG_IF_S(verbosity, cond)     VLOG_IF_S(verbosity, cond)
	#define DLOG_IF_S(verbosity_name, cond) LOG_IF_S(verbosity_name, cond)
	#define DVLOG_S(verbosity)              VLOG_S(verbosity)
	#define DLOG_S(verbosity_name)          LOG_S(verbosity_name)
#else
	// Debug logging disabled:
	#define DVLOG_IF_S(verbosity, cond)                                                     \
		(true || (verbosity) > aer::log::current_verbosity_cutoff() || (cond) == false)       \
			? (void)0                                                                       \
			: aer::log::Voidify() & aer::log::StreamLogger(verbosity, __FILE__, __LINE__)

	#define DLOG_IF_S(verbosity_name, cond) DVLOG_IF_S(aer::log::Verbosity_ ## verbosity_name, cond)
	#define DVLOG_S(verbosity)              DVLOG_IF_S(verbosity, true)
	#define DLOG_S(verbosity_name)          DVLOG_S(aer::log::Verbosity_ ## verbosity_name)
#endif

#if LOG_DEBUG_CHECKS
	// Debug checks enabled:
	#define DCHECK_S(cond)                  CHECK_S(cond)
	#define DCHECK_NOTNULL_S(x)             CHECK_NOTNULL_S(x)
	#define DCHECK_EQ_S(a, b)               CHECK_EQ_S(a, b)
	#define DCHECK_NE_S(a, b)               CHECK_NE_S(a, b)
	#define DCHECK_LT_S(a, b)               CHECK_LT_S(a, b)
	#define DCHECK_LE_S(a, b)               CHECK_LE_S(a, b)
	#define DCHECK_GT_S(a, b)               CHECK_GT_S(a, b)
	#define DCHECK_GE_S(a, b)               CHECK_GE_S(a, b)
#else
// Debug checks disabled:
	#define DCHECK_S(cond)                  CHECK_S(true || (cond))
	#define DCHECK_NOTNULL_S(x)             CHECK_S(true || (x) != nullptr)
	#define DCHECK_EQ_S(a, b)               CHECK_S(true || (a) == (b))
	#define DCHECK_NE_S(a, b)               CHECK_S(true || (a) != (b))
	#define DCHECK_LT_S(a, b)               CHECK_S(true || (a) <  (b))
	#define DCHECK_LE_S(a, b)               CHECK_S(true || (a) <= (b))
	#define DCHECK_GT_S(a, b)               CHECK_S(true || (a) >  (b))
	#define DCHECK_GE_S(a, b)               CHECK_S(true || (a) >= (b))
#endif

#endif // LOG_WITH_STREAMS