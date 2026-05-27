#include "motionbridge_api.h"

/* Dummy silnik wkompilowany statycznie. Gdy silnik idzie do DLL-a —
 * ta deklaracja odpada, a setPTR woła się z prawdziwym wskaźnikiem.  */
extern "C" int runTranslationFromYaml(char const* yaml_path);

int main(int argc, char *argv[])
{
    setPTR(&runTranslationFromYaml);
    return runGUI(argc, argv);
}
