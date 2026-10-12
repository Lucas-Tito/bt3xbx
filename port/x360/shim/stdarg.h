#ifndef X360_SHIM_STDARG_H
#define X360_SHIM_STDARG_H
/* passa para o cl como o dele: va_list é char * no MSVC; as macros do clang não servem ao cl */
typedef char *va_list;
#endif
