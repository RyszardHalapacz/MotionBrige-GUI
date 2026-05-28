/* ============================================================================
 *  motionbridge_api.h  —  Granica GUI ↔ silnik MotionBridge
 *
 *  Kontrakt C ABI:
 *    exe woła setPTR(&runTranslationFromYaml)
 *    exe woła runGUI(argc, argv)  — blokuje do zamknięcia okna
 *
 *  Silnik implementuje runTranslationFromYaml:
 *    - czyta  motionbridge_run.yaml
 *    - pisze  motionbridge_result.yaml + pliki SRC/DAT/logi
 *    - opcjonalnie dopisuje linia po linii do progress_file
 *    - zwraca 0=OK, 2=FAILED
 * ==========================================================================*/
#ifndef MOTIONBRIDGE_API_H
#define MOTIONBRIDGE_API_H

#ifdef _WIN32
  #ifdef MOTIONBRIDGE_GUI_EXPORTS
    #define MB_API __declspec(dllexport)
  #else
    #define MB_API __declspec(dllimport)
  #endif
#else
  #define MB_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*TranslationCallback)(char const* yaml_path);

MB_API void        setPTR(TranslationCallback cb);
MB_API void        setConfigPath(const char* path);
MB_API int         runGUI(int argc, char* argv[]);
MB_API const char* getVersion();

#ifdef __cplusplus
}
#endif

#endif /* MOTIONBRIDGE_API_H */
