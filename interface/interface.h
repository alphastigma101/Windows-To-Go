#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <windows.h>
namespace Interface {

	template<typename Derived>
	class CRTP {
		public:
			~CRTP() = default;
			static bool FileExists(const std::wstring& type) { return static_cast<Derived>(this)->FileExists(type); }
			static void ShowProgress() { return static_cast<Derived>(this)->ShowProgress(); }
			static void ShowError() { return static_cast<Derived>(this)->ShowError(); }
			static bool ConfirmUserIntent() { return static_cast<Derived>(this)->ConfirmUserIntent(); }
	};
};

#endif