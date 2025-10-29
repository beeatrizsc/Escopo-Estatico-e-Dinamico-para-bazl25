/* bazl25_debug.c
   Simulador passo-a-passo para ~bazL25~
   - Modo global de numeraçao de linhas
   - Mostra todo o arquivo a cada passo com seta -> na linha atual
   - Mostra pilha de ativaçao com end_ret (linha global de retorno)
   - Avalia argumentos ANTES de empilhar (evita erro com k/t)
   - Espera <Enter> a cada passo
   Compilar: gcc -std=c99 -O2 -o bazl25_debug bazl25_debug.c
   Rodar: .\bazl25_debug.exe    (no PowerShell use .\)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINHAS_FILE 500
#define MAX_FUNCOES 50
#define MAX_LINHAS_FUN 200

/* --- estruturas --- */

typedef struct {
    char text[200];
    int global_line;   /* número global da linha no arquivo */
} SourceLine;

/* funçao: nome, parametros, linhas, mapeamento de cada linha para número global */
typedef struct {
    char nome[32];
    char parametros[8][32];
    int qtd_param;
    char codigo[MAX_LINHAS_FUN][200];     /* texto de cada linha da funçao (trim) */
    int linha_global[MAX_LINHAS_FUN];     /* número global correspondente */
    int qtd_linhas;
} Funcao;

/* ativaçao na pilha */
typedef struct {
    Funcao *func;
    int pc;                 /* indice local dentro de func->codigo (0-based) */
    char nome_funcao[32];
    char var_names[40][32]; /* nomes das variaveis locais e parametros */
    int var_vals[40];
    int qtd_vars;
    int end_ret_global;     /* linha global para retornar (como no slide) */
} Ativacao;

/* --- memoria do programa --- */
SourceLine source[MAX_LINHAS_FILE];
int source_n = 0;

Funcao funcoes[MAX_FUNCOES];
int funcoes_n = 0;

Ativacao pilha[50];
int sp = -1; /* topo da pilha index */

int modo_estatico = 1; /* 1 = estatico (lexical), 0 = dinamico */

/* --- utilitarios --- */
void trim(char *s) {
    /* remove espaços inicias e finais */
    while(*s && isspace((unsigned char)*s)) memmove(s, s+1, strlen(s));
    while(strlen(s)>0 && isspace((unsigned char)s[strlen(s)-1])) s[strlen(s)-1]='\0';
}

/* imprime todo o source com seta na linha atual (global_line_current).
   se global_line_current==0 nao aponta para nada ainda.
*/
void print_source_with_arrow(int global_line_current) {
    printf("\n--- Codigo fonte (linhas globais) ---\n");
    for(int i=0;i<source_n;i++){
        if(source[i].global_line == global_line_current)
            printf("-> %2d  %s\n", source[i].global_line, source[i].text);
        else
            printf("   %2d  %s\n", source[i].global_line, source[i].text);
    }
    printf("-------------------------------------\n\n");
}

/* busca funçao por nome */
Funcao* buscar_funcao(const char *name) {
    for(int i=0;i<funcoes_n;i++) if(strcmp(funcoes[i].nome, name)==0) return &funcoes[i];
    return NULL;
}

/* procura variavel seguindo modo dinamico/estatico
   retorna 1 e coloca valor em *out se encontrada; 0 caso contrario.
   NOTA: para escopo estatico: procuramos apenas na ativaçao atual (topo)
         para escopo dinamico: procuramos de cima pra baixo na pilha.
*/
int procurar_var(int *out, const char *name) {
    if(!modo_estatico) {
        for(int i=sp;i>=0;i--){
            for(int j=0;j<pilha[i].qtd_vars;j++){
                if(strcmp(pilha[i].var_names[j], name)==0) { *out = pilha[i].var_vals[j]; return 1; }
            }
        }
    } else {
        if(sp>=0){
            for(int j=0;j<pilha[sp].qtd_vars;j++){
                if(strcmp(pilha[sp].var_names[j], name)==0) { *out = pilha[sp].var_vals[j]; return 1; }
            }
        }
    }
    /* globais */
    for(int i=0;i<funcoes_n;i++); /* noop to keep style */
    /* we stored globals as source top-level variables; let's search source for declared globals stored elsewhere */
    /* Simples: store globals in a special activation at bottom (index 0) if created; we'll check sp>=0 and bottom activation name "GLOBAL" */
    if(sp>=0 && strcmp(pilha[0].nome_funcao, "GLOBAL")==0) {
        for(int j=0;j<pilha[0].qtd_vars;j++){
            if(strcmp(pilha[0].var_names[j], name)==0) { *out = pilha[0].var_vals[j]; return 1; }
        }
    }
    return 0;
}

/* atribuir variavel: segue as regras: se existir em escopo visivel, atualiza; senao cria local (ou global se nao houver ativaçao) */
void atribuir_var(const char *name, int val) {
    if(!modo_estatico) {
        for(int i=sp;i>=0;i--){
            for(int j=0;j<pilha[i].qtd_vars;j++){
                if(strcmp(pilha[i].var_names[j], name)==0) { pilha[i].var_vals[j] = val; return; }
            }
        }
    } else {
        if(sp>=0){
            for(int j=0;j<pilha[sp].qtd_vars;j++){
                if(strcmp(pilha[sp].var_names[j], name)==0) { pilha[sp].var_vals[j] = val; return; }
            }
        }
    }
    /* nao encontrada: se existir ativaçao atual, cria local; senao cria global (no GLOBAL activation) */
    if(sp>=0) {
        strcpy(pilha[sp].var_names[pilha[sp].qtd_vars], name);
        pilha[sp].var_vals[pilha[sp].qtd_vars] = val;
        pilha[sp].qtd_vars++;
    } else {
        /* cria GLOBAL activation se nao existir */
    }
}

/* obter valor (gera erro se nao encontrada) */
int obter_valor(const char *name) {
    int v;
    if(procurar_var(&v, name)) return v;
    fprintf(stderr, "Erro: variavel '%s' nao declarada (busca de valor).\n", name);
    exit(1);
}

/* avalia expressao simples com operator + (ex: "z", "z+1", "z+1+2") */
int avaliar_expr_str(const char *expr) {
    char tmp[200];
    strncpy(tmp, expr, sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
    /* split por '+' */
    int acc = 0;
    char *p = strtok(tmp, "+");
    while(p) {
        trim(p);
        /* numero? */
        int isnum = 1;
        for(size_t i=0;i<strlen(p);i++){
            if(i==0 && p[i]=='-') continue;
            if(!isdigit((unsigned char)p[i])) { isnum = 0; break; }
        }
        if(isnum) acc += atoi(p);
        else acc += obter_valor(p);
        p = strtok(NULL, "+");
    }
    return acc;
}

/* mostra pilha formatada com end_ret quando presente */
void mostrar_pilha_status() {
    printf("Pilha:\n");
    if(sp < 0) { printf("  <vazia>\n"); return; }
    for(int i=0;i<=sp;i++){
        printf("  %s: ", pilha[i].nome_funcao);
        for(int j=0;j<pilha[i].qtd_vars;j++){
            printf("%s=%d", pilha[i].var_names[j], pilha[i].var_vals[j]);
            if(j+1 < pilha[i].qtd_vars) printf(", ");
        }
        /* mostrar end_ret so se diferente de zero */
        if(pilha[i].end_ret_global > 0) {
            printf("  end_ret=%d", pilha[i].end_ret_global);
        }
        printf("\n");
    }
}

/* cria ativaçao GLOBAL com variaveis globais (usado para armazenar 'var' do topo) */
void criar_ativacao_global() {
    sp++;
    strcpy(pilha[sp].nome_funcao, "GLOBAL");
    pilha[sp].qtd_vars = 0;
    pilha[sp].end_ret_global = 0;
}

/* cria uma ativaçao para funçao f (vazia) */
void push_ativacao(Funcao *f) {
    sp++;
    pilha[sp].func = f;
    strcpy(pilha[sp].nome_funcao, f->nome);
    pilha[sp].pc = 0;
    pilha[sp].qtd_vars = 0;
    pilha[sp].end_ret_global = 0;
}

/* pop */
void pop_ativacao() {
    if(sp>=0) sp--;
}

/* encontra a funcao que contém o global_line -> retorna func* e local index via out_local_index */
Funcao* find_func_by_global_line(int global_line, int *out_local_index) {
    for(int i=0;i<funcoes_n;i++){
        for(int j=0;j<funcoes[i].qtd_linhas;j++){
            if(funcoes[i].linha_global[j] == global_line) {
                if(out_local_index) *out_local_index = j;
                return &funcoes[i];
            }
        }
    }
    return NULL;
}

/* --- carregamento do arquivo: armazena source[], e popula funcoes[] com mapeamento global de linhas --- */
void carregar_arquivo(const char *nome_arquivo) {
    FILE *f = fopen(nome_arquivo, "r");
    if(!f){ fprintf(stderr,"Nao abriu %s\n", nome_arquivo); exit(1); }
    char buff[200];
    int global_ln = 0;
    source_n = 0;
    funcoes_n = 0;
    Funcao *atual = NULL;
    while(fgets(buff, sizeof(buff), f)) {
        global_ln++;
        char linha[200]; strcpy(linha, buff);
        trim(linha);
        if(strlen(linha)==0) {
            /* guardar linha vazia também para manter contagem? o slide ignora vazias; aqui vamos ignorar linhas vazias mas manter numeracao continua */
            /* para simplicidade, nao adicionamos vazias ao source; mas ainda avançamos global_ln */
        }
        /* guardar a versao trimmed no source[] (assincronamente: guardamos todas as linhas significativas) */
        strcpy(source[source_n].text, linha);
        source[source_n].global_line = global_ln;
        source_n++;

        if(strncmp(linha, "var ", 4) == 0 && atual == NULL) {
            /* global var: adicionaremos depois na GLOBAL activation */
            /* para referência, a linha ja esta no source */
            continue;
        } else if(strncmp(linha, "func ", 5) == 0) {
            atual = &funcoes[funcoes_n++];
            trim(linha + 5);
            /* extrair nome e parametros */
            char namepart[100]; strcpy(namepart, linha+5);
            char *popen = strchr(namepart, '(');
            char *pclose = strrchr(namepart, ')');
            if(!popen || !pclose) { fprintf(stderr,"Erro sintaxe func header\n"); exit(1); }
            int nlen = popen - namepart;
            strncpy(atual->nome, namepart, nlen); atual->nome[nlen] = '\0'; trim(atual->nome);
            /* params */
            char paramsbuf[100]; int plen = pclose - popen - 1;
            if(plen > 0) {
                strncpy(paramsbuf, popen+1, plen); paramsbuf[plen] = '\0';
            } else paramsbuf[0] = '\0';
            atual->qtd_param = 0;
            if(strlen(paramsbuf)>0) {
                char *tok = strtok(paramsbuf, ",");
                while(tok) { trim(tok); strcpy(atual->parametros[atual->qtd_param++], tok); tok = strtok(NULL, ","); }
            }
            atual->qtd_linhas = 0;
        } else if(strcmp(linha, "}") == 0) {
            atual = NULL;
        } else {
            if(atual) {
                strcpy(atual->codigo[atual->qtd_linhas], linha);
                atual->linha_global[atual->qtd_linhas] = global_ln;
                atual->qtd_linhas++;
            }
        }
    }
    fclose(f);

    /* Criar ativaçao GLOBAL e preencher globais a partir das linhas top-level 'var ' */
    criar_ativacao_global();
    for(int i=0;i<source_n;i++){
        if(strncmp(source[i].text, "var ", 4) == 0) {
            char copia[200]; strcpy(copia, source[i].text + 4);
            char *tok = strtok(copia, ",");
            while(tok) {
                trim(tok);
                int ja_existe = 0;
                for (int j = 0; j < pilha[0].qtd_vars; j++) {
                    if (strcmp(pilha[0].var_names[j], tok) == 0) {
                        ja_existe = 1;
                    break;
                    }
                }
                if (!ja_existe) {
                    strcpy(pilha[0].var_names[pilha[0].qtd_vars], tok);
                    pilha[0].var_vals[pilha[0].qtd_vars] = 0;
                    pilha[0].qtd_vars++;
                }
                tok = strtok(NULL, ",");
            }
        }
    }
}

/* --- execuçao passo a passo com pc em ativaçao --- */
void executar_passo() {
    if(sp < 0) return;
    Ativacao *cur = &pilha[sp];
    Funcao *f = cur->func;
    /* se func==NULL -> trata-se do caso de MAIN inicial que criamos com func apontando para main; garantimos func set antes */
    if(cur->pc >= f->qtd_linhas) {
        /* fim da funçao: desempilha */
        pop_ativacao();
        return;
    }

    /* determinar a global line da instruçao atual */
    int global_ln = f->linha_global[cur->pc];
    /* imprimir o codigo inteiro com seta */
    print_source_with_arrow(global_ln);

    /* mostrar o comando atual */
    char linha_atual[200]; strcpy(linha_atual, f->codigo[cur->pc]);
    trim(linha_atual);
    printf("Comando: %s // linha %d\n\n", linha_atual, global_ln);

    /* agora executar a linha */
    if(strncmp(linha_atual, "var ", 4) == 0) {
    char copia[200]; strcpy(copia, linha_atual + 4);
    char *tok = strtok(copia, ",");
    while(tok) {
        trim(tok);
        strcpy(cur->var_names[cur->qtd_vars], tok);
        cur->var_vals[cur->qtd_vars] = 0;  /* valor padrão, mas ainda não mostrado */
        cur->qtd_vars++;
        tok = strtok(NULL, ",");
    }

    /* Mostra pilha com variáveis recém-criadas, mas sem valores */
    printf("Pilha:\n");
    for(int i=0; i<=sp; i++) {
        printf("  %s: ", pilha[i].nome_funcao);
        for(int j=0; j<pilha[i].qtd_vars; j++) {
            if (i == sp && j >= cur->qtd_vars - 1)  /* variáveis recém-declaradas */
                printf("%s", pilha[i].var_names[j]);
            else
                printf("%s=%d", pilha[i].var_names[j], pilha[i].var_vals[j]);

            if(j + 1 < pilha[i].qtd_vars) printf(", ");
        }
        if(pilha[i].end_ret_global > 0)
            printf("  end_ret=%d", pilha[i].end_ret_global);
        printf("\n");
    }

    printf("Pressione <enter> para continuar...\n");
    getchar();
    cur->pc++;
    return;
}


    if(strncmp(linha_atual, "print ", 6) == 0) {
        char name[64]; strcpy(name, linha_atual+6); trim(name);
        int v = obter_valor(name);
        printf("Saida: %s = %d\n\n", name, v);
        mostrar_pilha_status();
        getchar();
        cur->pc++;
        return;
    }

    /* atribuiçao sem chamada (x = expr) */
    if(strchr(linha_atual, '=') && strchr(linha_atual, '(') == NULL) {
        char lhs[64], rhs[200];
        /* cuidado com espaços ao redor do = */
        char *eq = strchr(linha_atual, '=');
        if(!eq) { cur->pc++; return; }
        int lenl = eq - linha_atual;
        strncpy(lhs, linha_atual, lenl); lhs[lenl] = '\0'; trim(lhs);
        strcpy(rhs, eq+1); trim(rhs);
        int val = avaliar_expr_str(rhs);
        atribuir_var(lhs, val);
        mostrar_pilha_status();
        getchar();
        cur->pc++;
        return;
    }

    /* chamada de funçao: name(arg,arg) -> importante: avalia argumentos ANTES de empilhar */
    if(strchr(linha_atual, '(')) {
        char fname[64], argsbuf[200];
        sscanf(linha_atual, " %[^ (](%[^)])", fname, argsbuf);
        trim(fname); trim(argsbuf);
        Funcao *ch = buscar_funcao(fname);
        if(!ch) { fprintf(stderr,"Erro: funcao %s nao encontrada\n", fname); exit(1); }

        /* parse args into strings */
        char args_copy[200]; strcpy(args_copy, argsbuf);
        char *tk = strtok(args_copy, ",");
        int nargs = 0;
        char arg_tokens[8][200];
        while(tk && nargs < 8) { strcpy(arg_tokens[nargs++], tk); tk = strtok(NULL, ","); }

        /* avaliar argumentos no contexto do chamador (antes do push) */
        int arg_vals[8];
        for(int a=0;a<nargs;a++){
            trim(arg_tokens[a]);
            arg_vals[a] = avaliar_expr_str(arg_tokens[a]);
        }

        /* calcular end_ret_global: é a proxima linha global apos a chamada */
        int caller_global_line = f->linha_global[cur->pc];
        int end_ret = caller_global_line + 1;

        /* avançar o pc do chamador (de modo que ao retornar continue na proxima instruçao) */
        cur->pc++;

        /* empilhar nova ativaçao e copiar parametros */
        push_ativacao(ch);
        Ativacao *newa = &pilha[sp];
        newa->func = ch;
        newa->pc = 0;
        newa->qtd_vars = 0;
        newa->end_ret_global = end_ret;
        /* copiar parametros */
        for(int p=0;p<ch->qtd_param;p++) {
            strcpy(newa->var_names[newa->qtd_vars], ch->parametros[p]);
            newa->var_vals[newa->qtd_vars] = (p < nargs ? arg_vals[p] : 0);
            newa->qtd_vars++;
        }

        /* mostrar pilha com end_ret e esperar enter antes de entrar */
        mostrar_pilha_status();
        getchar();

        return; /* nao incrementamos aqui: execuçao continuara no proximo passo com a nova ativaçao no topo */
    }

    /* se nada reconhecido, apenas pular */
    cur->pc++;
    mostrar_pilha_status();
    getchar();
}

/* rotina principal: executa passo-a-passo até pilha conter so GLOBAL (ou ficar vazia) */
void executar_loop() {
    while(sp > 0) { /* sp==0 é GLOBAL; quando main terminar popara para 0 e saimos */
        executar_passo();
    }
}

/* ---- main ---- */
int main() {
    char filename[200];
    char modo[20];

    printf("=== Simulador ~bazL25~ (modo debug) ===\n");
    printf("Nome do arquivo (.bazl25) [enter = exemplo.bazl25]: ");
    if(!fgets(filename, sizeof(filename), stdin)) return 0;
    trim(filename);
    if(strlen(filename)==0) strcpy(filename, "exemplo.bazl25");

    printf("Modo de escopo [static/dynamic] (enter = static): ");
    if(!fgets(modo, sizeof(modo), stdin)) return 0;
    trim(modo);
    modo_estatico = (strcmp(modo, "dynamic") != 0);

    /* carregar */
    carregar_arquivo(filename);

    /* localizar main */
    Funcao *mainf = buscar_funcao("main");
    if(!mainf) { fprintf(stderr,"Erro: main() nao encontrada\n"); return 1; }

    /* empilhar ativacao MAIN (apos GLOBAL que ja existe no index 0) */
    push_ativacao(mainf);
    pilha[sp].func = mainf;
    pilha[sp].pc = 0;
    pilha[sp].qtd_vars = 0;
    pilha[sp].end_ret_global = 0;

    printf("\nExecutando em modo %s...\n\n", modo_estatico ? "ESTATICO" : "DINAMICO");
    /* mostrar codigo e pilha inicialmente (nenhuma linha marcada) */
    print_source_with_arrow(0);
    mostrar_pilha_status();
    printf("\nDigite <enter> para iniciar...");
    getchar();

    /* loop passo-a-passo */
    executar_loop();

    printf("\nExecucao finalizada.\n");
    return 0;
}
