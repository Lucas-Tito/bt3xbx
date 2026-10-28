/* Cadeia do r1 no ABI do XDK: 0(r1) guarda o r1 anterior; o LR salvo fica em anterior-8.
   A leitura é fornecida pelo filtro para proteger cada acesso com SEH. */
typedef int (*Pilha360_Ler)(unsigned endereco, unsigned *valor);
static int Pilha360_Proximo(unsigned inicio, unsigned atual, Pilha360_Ler ler,
                            unsigned *anterior, unsigned *retorno) {
    if ((atual & 15) != 0 || atual < inicio || atual - inicio > 16 * 1024 * 1024 ||
        !ler(atual, anterior) || *anterior <= atual || (*anterior & 15) != 0 ||
        *anterior - inicio > 16 * 1024 * 1024 || !ler(*anterior - 8, retorno)) {
        return 0;
    }
    return 1;
}
