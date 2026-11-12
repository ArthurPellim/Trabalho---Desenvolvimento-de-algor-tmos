/*
 * EXCHANGE DE CRIPTOMOEDAS - Projeto FEI
 * Linguagem: C
 *
 * Arquivos de dados:
 *   usuarios.txt -> nome;cpf;senha;reais;btc;eth;xrp
 *   cotacoes.txt -> btc;eth;xrp
 *   extrato.txt  -> cpf;data;sinal;valor;moeda;cotacao;taxa;reais;btc;eth;xrp
 *
 * Compilar:  gcc main.c -o main.exe -Wall
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
 
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
 
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
 
/* ===================== CONSTANTES ===================== */
 
#define MAX_USUARIOS 100
#define ARQ_USUARIOS "usuarios.txt"
#define ARQ_COTACOES "cotacoes.txt"
#define ARQ_EXTRATO  "extrato.txt"
#define ARQ_TEMP     "usuarios.tmp"
 
/* Limite maximo aceito para qualquer valor digitado */
#define VALOR_MAXIMO 1e12
 
/* Indices dos saldos na carteira */
#define REAL 0
#define BTC  1
#define ETH  2
#define XRP  3
 
/* Indices das cotacoes (cotacao[i] corresponde a saldo[i + 1]) */
#define NUM_MOEDAS 3
 
static const char  *NOME_MOEDA[NUM_MOEDAS]  = {"Bitcoin", "Ethereum", "Ripple"};
static const char  *SIGLA_MOEDA[NUM_MOEDAS] = {"BTC", "ETH", "XRP"};
static const double TAXA_COMPRA[NUM_MOEDAS] = {0.02, 0.01, 0.01};
static const double TAXA_VENDA[NUM_MOEDAS]  = {0.03, 0.02, 0.01};
static const double COTACAO_INICIAL[NUM_MOEDAS] = {350000.00, 16108.00, 2.57};
 
/* ===================== ESTRUTURAS ===================== */
 
typedef struct {
    char   nome[100];
    char   cpf[12];      /* 11 digitos + '\0' */
    char   senha[7];     /* 6 digitos + '\0' */
    double saldo[4];     /* REAL, BTC, ETH, XRP */
} Usuario;
 
/* ===================== VARIAVEIS GLOBAIS ===================== */
 
static Usuario usuarios[MAX_USUARIOS];
static int     totalUsuarios = 0;
static double  cotacao[NUM_MOEDAS];
static int     terminalReal = 0;   /* 1 se a saida for um terminal que aceita limpar a tela */
 
/* ===================== UTILIDADES ===================== */
 
/* Arredonda para 2 casas decimais (centavos) */
static double arred2(double v) {
    return floor(v * 100.0 + 0.5) / 100.0;
}
 
/* Arredonda para 8 casas decimais (precisao das criptomoedas).
   Mantem a memoria identica ao que e gravado no arquivo e mostrado na tela. */
static double arred8(double v) {
    return floor(v * 1e8 + 0.5) / 1e8;
}
 
/* Le uma linha do teclado sem o '\n'. Retorna 0 se o teclado fechou (EOF).
   Se a linha digitada for maior que o buffer, descarta o resto para nao
   contaminar a proxima leitura. */
static int lerLinha(const char *prompt, char *buf, size_t n) {
    printf("%s", prompt);
    if (fgets(buf, (int)n, stdin) == NULL) {
        buf[0] = '\0';
        return 0;
    }
    size_t len = strcspn(buf, "\r\n");
    if (buf[len] == '\0') {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { }
    }
    buf[len] = '\0';
 
    /* remove espacos no fim e no inicio (ex.: "tudo " vira "tudo") */
    while (len > 0 && isspace((unsigned char)buf[len - 1])) buf[--len] = '\0';
    size_t ini = 0;
    while (buf[ini] && isspace((unsigned char)buf[ini])) ini++;
    if (ini > 0) memmove(buf, buf + ini, strlen(buf + ini) + 1);
 
    return 1;
}
 
/* Detecta se a saida e um terminal real e, no Windows, ativa os codigos ANSI */
static void iniciarTerminal(void) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD modo = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &modo))
        terminalReal = SetConsoleMode(h, modo | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
    terminalReal = isatty(1);
#endif
}
 
/* Limpa a tela usando codigos ANSI. Fora de um terminal real, nao faz nada. */
static void limparTela(void) {
    if (!terminalReal) return;
    printf("\033[2J\033[3J\033[H");
    fflush(stdout);
}
 
/* Espera o usuario apertar Enter para ele conseguir ler o resultado */
static void pausar(void) {
    char buf[16];
    lerLinha("\nPressione Enter para continuar...", buf, sizeof(buf));
}
 
/* Converte um texto em numero real positivo (aceita virgula ou ponto).
   Rejeita "inf", "nan", zero, negativos e valores absurdos.
   Retorna 1 se valido. */
static int converterValor(const char *texto, double *valor) {
    char buf[64], *fim;
    strncpy(buf, texto, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
 
    for (char *p = buf; *p; p++)
        if (*p == ',') *p = '.';
 
    double v = strtod(buf, &fim);
    while (*fim == ' ') fim++;
    if (fim == buf || *fim != '\0' || !isfinite(v) || v <= 0 || v > VALOR_MAXIMO) {
        printf("Valor invalido. Digite um numero maior que zero.\n");
        return 0;
    }
    *valor = v;
    return 1;
}
 
/* Le um numero real positivo do teclado. Retorna 1 se valido. */
static int lerValor(const char *prompt, double *valor) {
    char buf[64];
    if (!lerLinha(prompt, buf, sizeof(buf))) return 0;
    return converterValor(buf, valor);
}
 
/* Verifica se a string tem exatamente 'tam' digitos */
static int soDigitos(const char *s, size_t tam) {
    if (strlen(s) != tam) return 0;
    for (size_t i = 0; i < tam; i++)
        if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}
 
/* Remove '.' e '-' (permite digitar o CPF com pontuacao) */
static void removerPontuacaoCpf(char *s) {
    char *dst = s;
    for (char *src = s; *src; src++)
        if (*src != '.' && *src != '-')
            *dst++ = *src;
    *dst = '\0';
}
 
/* Compara duas strings ignorando maiusculas/minusculas */
static int igualSemCaixa(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}
 
/* Escreve um double formatado com virgula decimal (ex.: 580,00) */
static void fmtVirgula(char *dst, size_t n, const char *fmt, double v) {
    snprintf(dst, n, fmt, v);
    for (char *p = dst; *p; p++)
        if (*p == '.') *p = ',';
}
 
/* Data e hora atual no formato dd-mm-aaaa hh:mm */
static void dataHoraAtual(char *dst, size_t n) {
    time_t agora = time(NULL);
    struct tm *t = localtime(&agora);
    strftime(dst, n, "%d-%m-%Y %H:%M", t);
}
 
/* CPF 12345678900 -> 123.456.789-00 */
static void formatarCpf(const char *cpf, char *dst, size_t n) {
    snprintf(dst, n, "%.3s.%.3s.%.3s-%.2s", cpf, cpf + 3, cpf + 6, cpf + 9);
}
 
/* ===================== PERSISTENCIA ===================== */
 
static void carregarUsuarios(void) {
    FILE *f = fopen(ARQ_USUARIOS, "r");
    char linha[512];
    totalUsuarios = 0;
    if (!f) return;
 
    while (totalUsuarios < MAX_USUARIOS && fgets(linha, sizeof(linha), f)) {
        Usuario u;
        int lidos = sscanf(linha, "%99[^;];%11[^;];%6[^;];%lf;%lf;%lf;%lf",
                           u.nome, u.cpf, u.senha,
                           &u.saldo[REAL], &u.saldo[BTC],
                           &u.saldo[ETH], &u.saldo[XRP]);
        if (lidos == 7) {
            /* Garante que a memoria comeca com a mesma precisao do arquivo */
            u.saldo[REAL] = arred2(u.saldo[REAL]);
            u.saldo[BTC]  = arred8(u.saldo[BTC]);
            u.saldo[ETH]  = arred8(u.saldo[ETH]);
            u.saldo[XRP]  = arred8(u.saldo[XRP]);
            usuarios[totalUsuarios++] = u;
        }
    }
    fclose(f);
}
 
/* Grava em arquivo temporario e depois substitui o original,
   para nao corromper os dados se o programa fechar no meio. */
static int salvarUsuarios(void) {
    FILE *f = fopen(ARQ_TEMP, "w");
    if (!f) {
        printf("ERRO: nao foi possivel gravar os dados.\n");
        return 0;
    }
    for (int i = 0; i < totalUsuarios; i++) {
        Usuario *u = &usuarios[i];
        fprintf(f, "%s;%s;%s;%.2f;%.8f;%.8f;%.8f\n",
                u->nome, u->cpf, u->senha,
                u->saldo[REAL], u->saldo[BTC], u->saldo[ETH], u->saldo[XRP]);
    }
    fclose(f);
    remove(ARQ_USUARIOS);
    if (rename(ARQ_TEMP, ARQ_USUARIOS) != 0) {
        printf("ERRO: nao foi possivel atualizar %s.\n", ARQ_USUARIOS);
        return 0;
    }
    return 1;
}
 
static void salvarCotacoes(void) {
    FILE *f = fopen(ARQ_COTACOES, "w");
    if (!f) {
        printf("ERRO: nao foi possivel gravar as cotacoes.\n");
        return;
    }
    fprintf(f, "%.2f;%.2f;%.6f\n", cotacao[0], cotacao[1], cotacao[2]);
    fclose(f);
}
 
static void carregarCotacoes(void) {
    FILE *f = fopen(ARQ_COTACOES, "r");
    if (f && fscanf(f, "%lf;%lf;%lf", &cotacao[0], &cotacao[1], &cotacao[2]) == 3) {
        fclose(f);
        return;
    }
    if (f) fclose(f);
    /* Arquivo inexistente ou invalido: cria com valores iniciais */
    for (int i = 0; i < NUM_MOEDAS; i++) cotacao[i] = COTACAO_INICIAL[i];
    salvarCotacoes();
}
 
/* Acrescenta uma linha ao extrato com os saldos DEPOIS da operacao */
static void registrarExtrato(const Usuario *u, char sinal, double valor,
                             const char *moeda, double ct, double tx) {
    FILE *f = fopen(ARQ_EXTRATO, "a");
    char data[32];
    if (!f) {
        printf("ERRO: nao foi possivel gravar o extrato.\n");
        return;
    }
    dataHoraAtual(data, sizeof(data));
    fprintf(f, "%s;%s;%c;%.2f;%s;%.2f;%.2f;%.2f;%.8f;%.8f;%.8f\n",
            u->cpf, data, sinal, valor, moeda, ct, tx,
            u->saldo[REAL], u->saldo[BTC], u->saldo[ETH], u->saldo[XRP]);
    fclose(f);
}
 
/* ===================== AUTENTICACAO ===================== */
 
static Usuario *buscarPorCpf(const char *cpf) {
    for (int i = 0; i < totalUsuarios; i++)
        if (strcmp(usuarios[i].cpf, cpf) == 0)
            return &usuarios[i];
    return NULL;
}
 
/* Pede a senha e confere. Retorna 1 se correta. */
static int confirmarSenha(const Usuario *u) {
    char senha[64];
    if (!lerLinha("Digite sua senha (6 digitos): ", senha, sizeof(senha)))
        return 0;
    if (strcmp(senha, u->senha) != 0) {
        printf("Senha incorreta.\n");
        return 0;
    }
    return 1;
}
 
/* Login com ate 3 tentativas. Retorna o usuario ou NULL. */
static Usuario *efetuarLogin(void) {
    char cpf[64], senha[64];
 
    for (int tentativa = 1; tentativa <= 3; tentativa++) {
        if (!lerLinha("CPF (somente numeros): ", cpf, sizeof(cpf))) return NULL;
        if (!lerLinha("Senha (6 digitos): ", senha, sizeof(senha))) return NULL;
 
        removerPontuacaoCpf(cpf);
 
        Usuario *u = soDigitos(cpf, 11) ? buscarPorCpf(cpf) : NULL;
        if (u && soDigitos(senha, 6) && strcmp(u->senha, senha) == 0)
            return u;
 
        printf("CPF ou senha invalidos. (tentativa %d de 3)\n\n", tentativa);
    }
    return NULL;
}
 
/* ===================== OPERACOES ===================== */
 
static void mostrarSaldos(const Usuario *u) {
    char cpf[20], r[32], b[32], e[32], x[32];
    formatarCpf(u->cpf, cpf, sizeof(cpf));
    fmtVirgula(r, sizeof(r), "%.2f", u->saldo[REAL]);
    fmtVirgula(b, sizeof(b), "%.8f", u->saldo[BTC]);
    fmtVirgula(e, sizeof(e), "%.8f", u->saldo[ETH]);
    fmtVirgula(x, sizeof(x), "%.8f", u->saldo[XRP]);
 
    printf("\nNome: %s\n", u->nome);
    printf("CPF: %s\n", cpf);
    printf("Reais: %s\n", r);
    printf("Bitcoin: %s\n", b);
    printf("Ethereum: %s\n", e);
    printf("Ripple: %s\n", x);
}
 
static void consultarSaldo(const Usuario *u) {
    if (!confirmarSenha(u)) return;
    mostrarSaldos(u);
}
 
static void consultarExtrato(const Usuario *u) {
    FILE *f;
    char linha[512], cpf[20];
 
    if (!confirmarSenha(u)) return;
 
    formatarCpf(u->cpf, cpf, sizeof(cpf));
    printf("\nNome: %s\nCPF: %s\n\n", u->nome, cpf);
 
    f = fopen(ARQ_EXTRATO, "r");
    if (!f) {
        printf("Nenhuma operacao registrada.\n");
        return;
    }
 
    int encontrou = 0;
    while (fgets(linha, sizeof(linha), f)) {
        char cpfLinha[12], data[32], moeda[8], sinal;
        double valor, ct, tx, real, btc, eth, xrp;
 
        int lidos = sscanf(linha,
            "%11[^;];%31[^;];%c;%lf;%7[^;];%lf;%lf;%lf;%lf;%lf;%lf",
            cpfLinha, data, &sinal, &valor, moeda, &ct, &tx,
            &real, &btc, &eth, &xrp);
 
        if (lidos == 11 && strcmp(cpfLinha, u->cpf) == 0) {
            printf("%s %c %.2f %s CT: %.2f TX: %.2f REAL: %.2f BTC: %.8f ETH: %.8f XRP: %.8f\n",
                   data, sinal, valor, moeda, ct, tx, real,
                   btc, eth, xrp);
            encontrou = 1;
        }
    }
    fclose(f);
 
    if (!encontrou)
        printf("Nenhuma operacao registrada.\n");
}
 
static void depositar(Usuario *u) {
    double valor;
    if (!lerValor("Valor do deposito (R$): ", &valor)) return;
 
    valor = arred2(valor);
    if (valor < 0.01) {
        printf("Valor muito baixo. O minimo e R$ 0,01.\n");
        return;
    }
 
    u->saldo[REAL] = arred2(u->saldo[REAL] + valor);
    if (!salvarUsuarios()) return;
    registrarExtrato(u, '+', valor, "REAL", 0.0, 0.0);
 
    printf("\nDeposito realizado com sucesso!\n");
    mostrarSaldos(u);
}
 
static void sacar(Usuario *u) {
    double valor;
    if (!lerValor("Valor do saque (R$): ", &valor)) return;
 
    valor = arred2(valor);
    if (valor < 0.01) {
        printf("Valor muito baixo. O minimo e R$ 0,01.\n");
        return;
    }
 
    if (valor > u->saldo[REAL] + 1e-9) {
        printf("Saldo insuficiente. Saldo atual: R$ %.2f\n", u->saldo[REAL]);
        return;
    }
 
    u->saldo[REAL] = arred2(u->saldo[REAL] - valor);
    if (u->saldo[REAL] < 0) u->saldo[REAL] = 0;   /* evita -0,00 */
    if (!salvarUsuarios()) return;
    registrarExtrato(u, '-', valor, "REAL", 0.0, 0.0);
 
    printf("\nSaque realizado com sucesso!\n");
    mostrarSaldos(u);
}
 
static void mostrarCotacoes(void) {
    printf("\nCotacoes atuais:\n");
    for (int i = 0; i < NUM_MOEDAS; i++)
        printf("  %d - %-9s (%s): R$ %.2f   [compra %.0f%% | venda %.0f%%]\n",
               i + 1, NOME_MOEDA[i], SIGLA_MOEDA[i], cotacao[i],
               TAXA_COMPRA[i] * 100, TAXA_VENDA[i] * 100);
}
 
/* Le a escolha da moeda (1 a 3). Retorna indice 0..2 ou -1. */
static int escolherMoeda(const char *acao) {
    char buf[32], prompt[64];
    snprintf(prompt, sizeof(prompt), "Qual moeda deseja %s? (1-3): ", acao);
    if (!lerLinha(prompt, buf, sizeof(buf))) return -1;
    if (strlen(buf) != 1 || buf[0] < '1' || buf[0] > '3') {
        printf("Opcao invalida.\n");
        return -1;
    }
    return buf[0] - '1';
}
 
static void comprar(Usuario *u) {
    if (!confirmarSenha(u)) return;
 
    mostrarCotacoes();
    int m = escolherMoeda("comprar");
    if (m < 0) return;
 
    double valor;
    if (!lerValor("Quanto deseja comprar (valor em R$, sem a taxa): ", &valor)) return;
 
    valor = arred2(valor);
    if (valor < 0.01) {
        printf("Valor muito baixo. O minimo e R$ 0,01.\n");
        return;
    }
 
    double taxa      = TAXA_COMPRA[m];
    double valorTaxa = arred2(valor * taxa);
    double custo     = arred2(valor + valorTaxa);   /* valor + taxa */
 
    if (custo > u->saldo[REAL] + 1e-9) {
        printf("Saldo insuficiente. Custo total com taxa: R$ %.2f | Saldo: R$ %.2f\n",
               custo, u->saldo[REAL]);
        return;
    }
 
    double quantidade = arred8(valor / cotacao[m]);
    if (quantidade <= 0) {
        printf("Valor muito baixo para comprar essa moeda.\n");
        return;
    }
 
    u->saldo[REAL] = arred2(u->saldo[REAL] - custo);
    if (u->saldo[REAL] < 0) u->saldo[REAL] = 0;     /* evita -0,00 */
    u->saldo[m + 1] = arred8(u->saldo[m + 1] + quantidade);
 
    if (!salvarUsuarios()) return;
    registrarExtrato(u, '+', valor, SIGLA_MOEDA[m], cotacao[m], taxa);
 
    printf("\nCompra realizada: %.8f %s (taxa de R$ %.2f)\n",
           quantidade, SIGLA_MOEDA[m], valorTaxa);
    mostrarSaldos(u);
}
 
static void vender(Usuario *u) {
    if (!confirmarSenha(u)) return;
 
    mostrarCotacoes();
    int m = escolherMoeda("vender");
    if (m < 0) return;
 
    double saldoCripto = arred8(u->saldo[m + 1]);
    printf("Voce possui %.8f %s\n", saldoCripto, SIGLA_MOEDA[m]);
 
    if (saldoCripto <= 0) {
        printf("Voce nao possui %s para vender.\n", SIGLA_MOEDA[m]);
        return;
    }
 
    double quantidade;
    char prompt[96], texto[64];
    snprintf(prompt, sizeof(prompt),
             "Quanto deseja vender em %s (ou digite 'tudo'): ", SIGLA_MOEDA[m]);
    if (!lerLinha(prompt, texto, sizeof(texto))) return;
 
    if (igualSemCaixa(texto, "tudo")) {
        quantidade = saldoCripto;                 /* vende exatamente o saldo */
    } else {
        if (!converterValor(texto, &quantidade)) return;
        quantidade = arred8(quantidade);
        if (quantidade <= 0) {
            printf("Quantidade muito pequena. O minimo e 0,00000001 %s.\n",
                   SIGLA_MOEDA[m]);
            return;
        }
    }
 
    if (quantidade > saldoCripto) {
        printf("Saldo insuficiente de %s.\n", SIGLA_MOEDA[m]);
        return;
    }
 
    double taxa      = TAXA_VENDA[m];
    double bruto     = arred2(quantidade * cotacao[m]);
    double valorTaxa = arred2(bruto * taxa);
    double liquido   = arred2(bruto - valorTaxa);   /* desconta a taxa de venda */
 
    u->saldo[m + 1] = arred8(saldoCripto - quantidade);
    if (u->saldo[m + 1] < 0) u->saldo[m + 1] = 0;
    u->saldo[REAL] = arred2(u->saldo[REAL] + liquido);
 
    if (!salvarUsuarios()) return;
    registrarExtrato(u, '-', bruto, SIGLA_MOEDA[m], cotacao[m], taxa);
 
    printf("\nVenda realizada: %.8f %s, valor bruto R$ %.2f, taxa R$ %.2f, voce recebeu R$ %.2f\n",
           quantidade, SIGLA_MOEDA[m], bruto, valorTaxa, liquido);
    mostrarSaldos(u);
}
 
/* Varia cada cotacao aleatoriamente entre -5% e +5% */
static void atualizarCotacao(void) {
    printf("\nAtualizando cotacoes...\n");
    for (int i = 0; i < NUM_MOEDAS; i++) {
        double variacao = ((double)rand() / RAND_MAX) * 0.10 - 0.05; /* -0.05 a +0.05 */
        double antiga = cotacao[i];
        cotacao[i] = antiga * (1.0 + variacao);
        printf("  %-9s (%s): R$ %.2f -> R$ %.2f (%+.2f%%)\n",
               NOME_MOEDA[i], SIGLA_MOEDA[i], antiga, cotacao[i], variacao * 100);
    }
    salvarCotacoes();
}
 
/* ===================== MENU ===================== */
 
static void exibirMenu(const Usuario *u) {
    printf("\n========== MENU ==========\n");
    printf("Usuario: %s\n", u->nome);
    printf("--------------------------\n");
    printf("1. Consultar saldo\n");
    printf("2. Consultar extrato\n");
    printf("3. Depositar\n");
    printf("4. Sacar\n");
    printf("5. Comprar criptomoedas\n");
    printf("6. Vender criptomoedas\n");
    printf("7. Atualizar cotacao\n");
    printf("8. Sair\n");
    printf("==========================\n");
}
 
static void menuPrincipal(Usuario *u) {
    char buf[32];
    int opcao;
 
    do {
        limparTela();
        exibirMenu(u);
        if (!lerLinha("Escolha uma opcao: ", buf, sizeof(buf))) return;
        opcao = (strlen(buf) == 1) ? buf[0] - '0' : 0;
 
        if (opcao >= 1 && opcao <= 7)
            limparTela();
 
        switch (opcao) {
            case 1: consultarSaldo(u);   pausar(); break;
            case 2: consultarExtrato(u); pausar(); break;
            case 3: depositar(u);        pausar(); break;
            case 4: sacar(u);            pausar(); break;
            case 5: comprar(u);          pausar(); break;
            case 6: vender(u);           pausar(); break;
            case 7: atualizarCotacao();  pausar(); break;
            case 8: printf("\nAte logo, %s!\n", u->nome); break;
            default: printf("Opcao invalida.\n"); pausar();
        }
    } while (opcao != 8);
}
 
/* ===================== MAIN ===================== */
 
int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   /* acentos corretos no terminal do Windows */
#endif
    iniciarTerminal();
    srand((unsigned)time(NULL));
 
    carregarUsuarios();
    carregarCotacoes();
 
    if (totalUsuarios == 0) {
        printf("Nenhum usuario encontrado em %s.\n", ARQ_USUARIOS);
        printf("Crie o arquivo com linhas no formato:\n");
        printf("nome;cpf;senha;reais;btc;eth;xrp\n");
        return 1;
    }
 
    printf("====== EXCHANGE DE CRIPTOMOEDAS ======\n\n");
 
    Usuario *logado = efetuarLogin();
    if (!logado) {
        printf("\nAcesso negado.\n");
        return 1;
    }
 
    menuPrincipal(logado);
    return 0;
}
