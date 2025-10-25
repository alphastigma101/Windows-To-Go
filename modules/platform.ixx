export module platform;

import <string_view>;

template<typename CharT =
    #if IS_WINDOWS
        wchar_t
    #else
        char
    #endif
>

using basic_platform_string_view = std::basic_string_view<CharT>;
export basic_platform_string_view;

/*template<typename ReturnType =
    #if IS_WINDOWS
        std::wstring_view
    #else
       std::string_view
    #endif
>*/

// typename is the same as typedef
//using return_string_type = ReturnType;
//export return_string_type;

/*export return_string_type ConvertType(basic_platform_string_view& view) {
    #if IS_WINDOWS == 0
        return std::string_view(view);
    #endif
    return std::wstring_view(view);
}*/


/*export static basic_platform_string_view WideToNarrow(const auto wideStr) {
    //int narrowSize = WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowStr(narrowSize, 0);
    WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &narrowStr[0], narrowSize, nullptr, nullptr);
    return narrowStr;
}*/