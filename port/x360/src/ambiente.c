/* A CRT do Xbox 360 não tem getenv. O port de PC usa variáveis de ambiente como chaves de
   experimento (BT3_*); aqui todas valem "não definida". */
#include <stddef.h>

char *getenv(const char *nome) {
    (void)nome;
    return NULL;
}
