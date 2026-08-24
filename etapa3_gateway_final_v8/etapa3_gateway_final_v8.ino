/* ============================================================================
   PROJETO GARCOM  -  ETAPA 3.1 FINAL v7: OTA COM TRAVA DE SEGURANCA (corrigida)
   ESP8266 Lolin / Wemos D1 mini

   *** O QUE MUDOU DO v6 PRO v7 (bug real, achado em hardware 24/08/2026) ***

   O v6 usava ESPhttpUpdate.rebootOnUpdate(true). Confirmado direto na
   fonte (ESP8266httpUpdate.cpp, core 3.1.2): com isso ligado, a PROPRIA
   biblioteca chama ESP.restart() de dentro do update(), ANTES de devolver
   o controle pro nosso codigo - entao o EEPROM.put()/commit() que marca a
   versao como "resolvida" (o coracao da trava de seguranca) NUNCA
   RODAVA em caso de sucesso. Resultado visto em hardware: o Gateway
   baixava e gravava com sucesso, reiniciava, e no boot seguinte a EEPROM
   ainda dizia "nenhuma versao resolvida" - entao tentava se autoatualizar
   nesse MESMO conteudo de novo, pra sempre, mesmo em caso de sucesso.
   v7 corrige com rebootOnUpdate(false): agora update() so retorna
   HTTP_UPDATE_OK sem reiniciar sozinho, o codigo grava e CONFIRMA a EEPROM,
   e SO DEPOIS chama ESP.restart() explicitamente. Ver a funcao
   verificarAtualizacaoOTA() e a secao 14 do documento do projeto pro
   relato completo do incidente que revelou esse bug.

   POR QUE ESTE ARQUIVO EXISTE:

   Mescla o que ja estava 100% validado em hardware:
     - etapa3_gateway_final_v3.ino: WiFiManager + tema neon do portal + botao
       D5 de reconfiguracao + ESP-NOW + mDNS + POST pro Flask (Etapa 3.1,
       fechada, "passou na media" com o usuario).
     - etapa3_3_ota_teste.ino: checagem de firmware novo no GitHub e
       autoatualizacao via ESPhttpUpdate (Etapa 3.3) - mecanismo de
       download+gravacao+reboot CONFIRMADO em hardware real (23/08/2026):
       o Gateway detectou uma versao diferente, baixou um .bin via HTTPS,
       gravou por cima do proprio firmware e reiniciou sozinho, sem travar.
       (O primeiro teste acabou gravando um .bin errado por engano no lado
       do repositorio, nao da logica de OTA em si - ver armadilha 20 no
       documento do projeto.)

   O QUE MUDOU DO v4 PRO v5 (unica diferenca, cosmetica/diagnostica):

   O v4 (e o v3 antes dele) tinha um `CANAL_ESPERADO` fixo em 1, usado so
   pra imprimir um aviso "*** ATENCAO: esperava canal 1..." quando o
   roteador estava em outro canal. Isso nunca controlou o radio de verdade
   (o Gateway sempre seguiu o canal do roteador, nunca fixou nada) - so
   virou um alarme cada vez mais falso conforme o roteador foi mudando de
   canal sozinho durante os testes (1 -> 4 -> 6, tudo na mesma tarde). Como
   a mesa (Etapa 3.4, ver etapa2d_transmissor_v2.ino) agora faz channel
   hopping + cache e se adapta sozinha a qualquer canal, esse aviso deixou
   de indicar um problema real. O v5 so troca o alarme por um log neutro
   ("canal do roteador agora : X") - nenhuma outra linha de logica mudou.

   NADA na logica de Wi-Fi/portal/ESP-NOW/mDNS/POST/botao mudou - e
   exatamente o etapa3_gateway_final_v3.ino, byte a byte, so com a checagem
   de OTA inserida numa posicao especifica do setup() (ver abaixo).

   ONDE A CHECAGEM OTA ENTRA (armadilha da ordem de inicializacao, mesma
   logica da secao 5 do documento do projeto - cada peca nasce sobre um
   estado ja estavel):

     1. wm.autoConnect(...)          -> Wi-Fi conectado, radio no canal certo
     2. WiFi.setSleepMode(...)       -> radio sempre acordado
     3. verificarAtualizacaoOTA()    -> *** NOVO *** so aqui, com Wi-Fi ja
                                         garantidamente de pe, e ANTES de
                                         qualquer coisa relacionada a
                                         ESP-NOW/mDNS nascer (se a OTA
                                         reiniciar a placa no meio, nada
                                         disso chegou a inicializar seria
                                         a toa)
     4. MDNS.begin() / esp_now_init() -> só depois da checagem OTA resolvida

   Se a checagem OTA achar firmware igual (ou falhar por qualquer motivo:
   sem Wi-Fi - impossivel aqui pois so roda depois do autoConnect ter dado
   certo -, GitHub fora do ar, version.txt vazio) ela so retorna e o boot
   segue normal pro resto do setup(). NUNCA trava, nunca impede o Gateway
   de funcionar.

   O QUE MUDOU DO v5 PRO v6 - duas coisas, pedidas pelo usuario depois de
   achar o processo de publicar update complicado demais e arriscado:

   1. VERSIONAMENTO AUTOMATICO: o version.txt no GitHub agora e gerado
      sozinho por uma GitHub Action (.github/workflows/atualizar-versao.yml
      no repositorio) toda vez que voce sobe um firmware.bin novo - calcula
      um hash do arquivo e escreve no version.txt. Voce NUNCA MAIS edita
      esse arquivo a mao, e a classe de erro que causou a armadilha 20
      (version.txt e firmware.bin dessincronizados por esquecimento) deixa
      de poder acontecer.

   2. TRAVA DE SEGURANCA CONTRA LOOP INFINITO: antes (v3/v4/v5), o Gateway
      comparava o version.txt remoto contra um FIRMWARE_VERSION fixo
      GRAVADO NO CODIGO - se voce esquecesse de manter os dois sincronizados
      (ou subisse o .bin errado), o Gateway ficaria tentando se
      autoatualizar EM TODO BOOT, pra sempre, sem nunca convergir. O v6
      substitui isso por um estado guardado na EEPROM (sobrevive a
      power-cycle, nao so a reboot): o Gateway lembra sozinho qual foi a
      ultima versao que ele ja "resolveu" (aplicou com sucesso, ou desistiu
      depois de 3 tentativas falhas). Uma versao nova so ganha ate 3
      tentativas (uma por boot) - depois disso, se continuar falhando, o
      Gateway PARA de tentar e so volta a tentar quando o version.txt
      apontar pra um valor diferente (ou seja, um firmware.bin realmente
      novo). Ver "Trava de seguranca da OTA" nos #define's abaixo e a
      funcao verificarAtualizacaoOTA() pra detalhe completo.

   FIRMWARE_VERSION continua existindo, mas agora e SO um rotulo pra humano
   ler no Serial - nao tem mais NENHUM efeito na logica de OTA. Pode deixar
   como esta, ou mudar quando quiser, sem se preocupar em sincronizar com
   nada no GitHub.

   *** GRAVAR ESTE ARQUIVO NO GATEWAY FICOU MAIS SIMPLES ***

   Nao existe mais nenhum passo de "atualize o version.txt antes de gravar"
   - o EEPROM.begin() na primeira execucao comeca vazio/sem estado, o que
   e tratado como "nada resolvido ainda". Se o version.txt do GitHub
   apontar pra alguma coisa nesse momento, o Gateway vai tentar aplicar
   (no maximo 3 vezes, protegido pela trava) - isso e o comportamento
   correto e esperado, nao precisa de nenhum passo manual antes.

   Resumo do que fazer:
     1. Grave este .ino no Gateway via USB (sem nenhum passo antes)
     2. Confirme no Serial: Wi-Fi conecta, OTA roda a checagem e mostra
        "ja estou na versao mais recente" OU tenta baixar/gravar (se o
        version.txt do GitHub apontar pra algo diferente do que ja tinha
        sido resolvido antes), ESP-NOW/mDNS/botao continuam normais
     3. Da em diante, publicar update e so isto: exportar o .bin, subir
        como firmware.bin no repositorio - o resto (version.txt + trava
        de tentativas) e automatico

   Placa           : LOLIN(WEMOS) D1 R2 & mini
   Core ESP8266    : 3.1.2 (API de OTA verificada contra esta versao)
   Upload Speed    : 115200
   Monitor Serial  : 115200
   ========================================================================== */

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <espnow.h>
#include <ESP8266httpUpdate.h>
#include <EEPROM.h>

extern "C" {
  #include <user_interface.h>
}

/* ------------------------------- AJUSTES ---------------------------------- */

#define PORTAL_NOME          "Garcom-Config"
#define PORTAL_SENHA         "12345678"
#define PORTAL_TIMEOUT_S     180
#define CONEXAO_TIMEOUT_S    20

#define SERVIDOR_IP_RESERVA     "192.168.3.109"
#define SERVIDOR_PORTA_RESERVA  5000

// CANAL_ESPERADO removido no v5: desde a Etapa 3.4 a mesa faz channel
// hopping + cache (acha o canal certo sozinha, nao depende mais de um
// numero fixo aqui) - e o proprio canal do roteador ja mudou 3x so
// durante os testes (1 -> 4 -> 6), entao um "esperado" fixo so ia gerar
// alarme falso toda vez. O Gateway sempre seguiu o roteador pra onde ele
// fosse (nunca fixava o radio - so o aviso no Serial era fixo). Ver
// secao 4 do doc do projeto.
#define TENTATIVAS_MDNS      6
#define SERVICO_MDNS         "garcom"
#define PROTOCOLO_MDNS       "tcp"

/* --- Botao de reconfiguracao (validado em hardware) --- */

#define PINO_BOTAO_RESET     D5       // GPIO14 no Lolin/Wemos D1 mini
#define TEMPO_RESET_MS       10000UL  // segurar 10s para confirmar
#define PISCA_LENTO_MS       400      // velocidade do pisca no INICIO do aperto
#define PISCA_RAPIDO_MS      40       // velocidade do pisca perto dos 10s

/* --- OTA via GitHub (mecanismo confirmado em hardware, Etapa 3.3) --- */

// v6: FIRMWARE_VERSION deixou de ter qualquer efeito na logica de OTA -
// e so um rotulo pra voce mesmo, aparece no boot no Serial. Pode mudar ou
// nao, nunca mais precisa bater com nada no GitHub (ver "trava de
// seguranca" abaixo, secao 14 do documento do projeto).
#define FIRMWARE_VERSION       "v8-compilado-por-action"

#define OTA_GITHUB_USER        "vinilima-br"
#define OTA_GITHUB_REPO        "garcom-firmware"
#define OTA_GITHUB_BRANCH      "main"

// --- Trava de seguranca da OTA (novo no v6) ---
// Guarda em EEPROM (sobrevive a power-cycle, diferente da RTC memory) qual
// foi a ultima versao do version.txt que o Gateway ja "resolveu" (aplicou
// com sucesso OU desistiu depois de esgotar as tentativas). Assim, ele
// NUNCA fica preso tentando gravar o mesmo firmware.bin pra sempre - no
// maximo tenta OTA_LIMITE_TENTATIVAS vezes (uma por boot) pra cada versao
// nova que aparecer no version.txt, e depois disso so ignora aquele valor
// ate o version.txt mudar de novo (ou seja, ate um firmware.bin realmente
// novo ser publicado).
#define OTA_EEPROM_MAGIC        0xB16B00B5UL
#define OTA_LIMITE_TENTATIVAS   3
#define OTA_VERSAO_TAM          40   // bytes reservados por string de versao na EEPROM

struct EstadoOTA {
  uint32_t magic;
  char     versaoResolvida[OTA_VERSAO_TAM];    // ja aplicada com sucesso, ou desistida - nao mexe mais nela
  char     versaoEmTentativa[OTA_VERSAO_TAM];  // versao "pendente" acumulando tentativas agora
  uint8_t  tentativasFalhas;                   // tentativas seguidas SEM sucesso, so da versaoEmTentativa
};

/* --- CSS do portal: tema NEON v3, jovem e responsivo --- */
/* SEM PROGMEM - ver armadilha 17 no documento do projeto. Fica na RAM.     */

const char PORTAL_CSS[] =
  "<style>"

  "*{box-sizing:border-box;}"

  "@media (prefers-reduced-motion: reduce){"
    "*{animation-duration:.001ms !important;animation-iteration-count:1 !important;"
      "transition-duration:.001ms !important;}"
  "}"

  /* fundo: quase preto, com dois brilhos radiais tipo reflexo de LED */
  "body{"
    "margin:0;min-height:100vh;"
    "padding:clamp(20px,6vw,40px) clamp(12px,4vw,20px) 48px;"
    "background:"
      "radial-gradient(circle at 12% 8%, rgba(0,246,255,.16) 0%, transparent 42%),"
      "radial-gradient(circle at 88% 92%, rgba(255,43,214,.14) 0%, transparent 46%),"
      "#05050b !important;"
    "background-attachment:fixed;"
    "font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,Arial,sans-serif !important;"
    "color:#eafcff !important;"
    "-webkit-font-smoothing:antialiased;"
    "animation:garcomFade .5s ease-out;"
  "}"
  "@keyframes garcomFade{from{opacity:0;transform:translateY(14px);}to{opacity:1;transform:translateY(0);}}"

  /* logo neon com gradiente animado (setCustomHeadElement so injeta no <head>, */
  /* entao a "logo" e simulada com pseudo-elemento antes do body)              */
  "body::before{"
    "content:'\\26A1  PAINEL DO GARCOM \\26A1';"
    "display:block;text-align:center;font-weight:900;"
    "font-size:clamp(16px,5vw,22px);letter-spacing:1px;text-transform:uppercase;"
    "margin:2px 0 26px;"
    "background:linear-gradient(90deg,#00f6ff,#a855f7,#ff2bd6,#00f6ff);"
    "background-size:300% auto;"
    "-webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent;"
    "animation:neonShift 4s linear infinite;"
  "}"
  "@keyframes neonShift{to{background-position:300% center;}}"

  /* o card: brilho pulsante ao redor, tipo letreiro neon respirando */
  ".wrap{"
    "max-width:min(440px,94vw) !important;margin:0 auto !important;"
    "background:linear-gradient(165deg,#12121d 0%,#0a0a13 100%) !important;"
    "border:1.5px solid rgba(0,246,255,.35) !important;"
    "border-radius:22px !important;"
    "padding:clamp(20px,5vw,30px) clamp(18px,5vw,26px) 30px !important;"
    "animation:cardGlow 3.2s ease-in-out infinite !important;"
  "}"
  "@keyframes cardGlow{"
    "0%,100%{box-shadow:0 18px 50px rgba(0,0,0,.55),0 0 20px rgba(0,246,255,.16),"
                        "0 0 46px rgba(255,43,214,.08),inset 0 1px 0 rgba(255,255,255,.03);}"
    "50%{box-shadow:0 18px 50px rgba(0,0,0,.55),0 0 34px rgba(0,246,255,.32),"
                    "0 0 70px rgba(255,43,214,.18),inset 0 1px 0 rgba(255,255,255,.03);}"
  "}"

  "h1{"
    "color:#ffffff !important;text-align:center !important;font-weight:800 !important;"
    "font-size:clamp(18px,4.6vw,21px) !important;margin:2px 0 4px !important;letter-spacing:.3px;"
    "text-shadow:0 0 8px rgba(0,246,255,.75),0 0 20px rgba(0,246,255,.35) !important;"
  "}"
  "h3{"
    "color:#9be8ff !important;text-align:center !important;font-weight:600 !important;"
    "font-size:13px !important;margin:0 0 20px !important;opacity:.8;letter-spacing:.2px;"
  "}"
  "hr{border:none !important;border-top:1px solid rgba(0,246,255,.16) !important;margin:20px 0 !important;}"

  /* mensagens de status - cor semantica preservada, agora com borda+brilho neon */
  ".msg{border-radius:14px !important;padding:12px 14px !important;font-size:14px !important;"
       "margin:0 0 16px !important;border:1px solid transparent !important;line-height:1.4 !important;"
       "font-weight:600 !important;}"
  ".msg.S{background:rgba(157,255,31,.10) !important;border-color:rgba(157,255,31,.5) !important;"
         "color:#c6ff6b !important;box-shadow:0 0 14px rgba(157,255,31,.18) !important;}"
  ".msg.D{background:rgba(255,43,110,.10) !important;border-color:rgba(255,43,110,.5) !important;"
         "color:#ff8fae !important;box-shadow:0 0 14px rgba(255,43,110,.18) !important;}"
  ".msg.P{background:rgba(0,246,255,.08) !important;border-color:rgba(0,246,255,.45) !important;"
         "color:#8fe9ff !important;box-shadow:0 0 14px rgba(0,246,255,.16) !important;}"

  /* lista de redes / links: barra neon na lateral, desliza e brilha no hover */
  ".wrap a{"
    "color:#eafcff !important;text-decoration:none !important;display:flex !important;"
    "align-items:center !important;justify-content:space-between !important;"
    "padding:13px 14px !important;margin:6px 0 !important;border-radius:12px !important;"
    "background:rgba(255,255,255,.03) !important;"
    "border:1px solid rgba(0,246,255,.14) !important;"
    "border-left:3px solid rgba(0,246,255,.55) !important;"
    "font-size:14px !important;font-weight:600 !important;"
    "transition:background .18s ease,transform .15s ease,border-color .18s ease,box-shadow .18s ease !important;"
  "}"
  ".wrap a:hover{"
    "background:rgba(0,246,255,.08) !important;border-color:rgba(255,43,214,.5) !important;"
    "border-left-color:#ff2bd6 !important;transform:translateX(4px) !important;"
    "box-shadow:0 0 18px rgba(255,43,214,.22) !important;"
  "}"
  ".wrap a:active{transform:translateX(4px) scale(.98) !important;}"

  "label{display:block !important;color:#9be8ff !important;font-size:11px !important;"
        "font-weight:800 !important;text-transform:uppercase !important;letter-spacing:1px !important;"
        "margin:18px 0 7px !important;}"
  "input[type=text],input[type=password]{"
    "width:100% !important;border-radius:12px !important;"
    "border:1.5px solid rgba(0,246,255,.28) !important;padding:13px !important;"
    "background:#0d0d16 !important;color:#eafcff !important;font-size:15px !important;"
    "transition:border-color .18s ease,box-shadow .18s ease !important;"
  "}"
  "input[type=text]:focus,input[type=password]:focus{"
    "outline:none !important;border-color:#00f6ff !important;"
    "box-shadow:0 0 0 3px rgba(0,246,255,.18),0 0 18px rgba(0,246,255,.35) !important;"
  "}"
  "input[type=checkbox]{accent-color:#ff2bd6 !important;width:16px !important;height:16px !important;}"

  /* botao: gradiente neon animado, brilho duplo (ciano + magenta), levanta no hover */
  "button,input[type=submit]{"
    "width:100% !important;"
    "background:linear-gradient(135deg,#00f6ff,#a855f7 50%,#ff2bd6) !important;"
    "background-size:200% auto !important;"
    "color:#050509 !important;border:none !important;border-radius:14px !important;"
    "padding:14px !important;font-weight:900 !important;font-size:15px !important;"
    "text-transform:uppercase !important;letter-spacing:.6px !important;"
    "margin-top:18px !important;cursor:pointer !important;"
    "box-shadow:0 0 22px rgba(0,246,255,.30),0 0 40px rgba(255,43,214,.18) !important;"
    "transition:background-position .25s ease,box-shadow .18s ease,transform .15s ease !important;"
  "}"
  "button:hover,input[type=submit]:hover{"
    "background-position:100% center !important;"
    "box-shadow:0 0 28px rgba(0,246,255,.45),0 0 56px rgba(255,43,214,.30) !important;"
    "transform:translateY(-2px) !important;"
  "}"
  "button:active,input[type=submit]:active{transform:translateY(0) scale(.97) !important;}"

  "::-webkit-scrollbar{width:8px;}"
  "::-webkit-scrollbar-thumb{background:linear-gradient(#00f6ff,#ff2bd6);border-radius:8px;}"

  /* responsivo: mais respiro e melhor centralizacao em telas maiores */
  "@media (min-width:480px){"
    ".wrap{padding:34px 34px 36px !important;}"
    "body::before{font-size:24px;margin-bottom:30px;}"
  "}"
  "@media (min-width:820px){"
    "body{display:flex;align-items:flex-start;justify-content:center;}"
    ".wrap{max-width:460px !important;margin-top:20px !important;}"
  "}"

  "</style>";

/* ---------------------- ESTRUTURA DO PACOTE ESP-NOW ---------------------- */

typedef struct __attribute__((packed)) {
  uint8_t  mac[6];
  uint32_t contador;
} PacoteChamado;

/* --------------------------- FILA DE CHAMADOS ---------------------------- */

#define FILA_TAM 8

struct ItemFila {
  uint8_t  mac[6];
  uint32_t contador;
};

ItemFila          fila[FILA_TAM];
volatile uint8_t  fila_entrada = 0;
volatile uint8_t  fila_saida   = 0;
volatile uint32_t descartados  = 0;

/* --------------------------- CONTADORES ---------------------------------- */

String   url_servidor;
bool     achou_por_mdns = false;
uint32_t recebidos = 0;
uint32_t ok_201    = 0;
uint32_t ok_200    = 0;
uint32_t falhas    = 0;

unsigned long ultimo_relatorio = 0;

/* --------------------------- FUNCOES AUXILIARES -------------------------- */

void macParaTexto(const uint8_t *mac, char *saida) {
  snprintf(saida, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void ledOn()  { digitalWrite(LED_BUILTIN, LOW);  }  // ESP8266: LOW = aceso
void ledOff() { digitalWrite(LED_BUILTIN, HIGH); }  // ESP8266: HIGH = apagado

/* --------------------------- OTA VIA GITHUB ------------------------------- */
/* Mesma logica ja confirmada em hardware no etapa3_3_ota_teste.ino. Nunca
   trava e nunca impede o resto do setup() de continuar: qualquer falha so
   escreve no Serial e retorna. */

String otaVersionUrl() {
  return String("https://raw.githubusercontent.com/") + OTA_GITHUB_USER +
         "/" + OTA_GITHUB_REPO + "/" + OTA_GITHUB_BRANCH + "/version.txt";
}

String otaFirmwareUrl() {
  return String("https://raw.githubusercontent.com/") + OTA_GITHUB_USER +
         "/" + OTA_GITHUB_REPO + "/" + OTA_GITHUB_BRANCH + "/firmware.bin";
}

/* Copia uma String pro campo fixo do EstadoOTA, sempre terminando em \0 -
   nunca deixa passar do tamanho reservado, mesmo que venha algo maior. */
void copiarVersaoPraEstado(char *destino, const String &origem) {
  strncpy(destino, origem.c_str(), OTA_VERSAO_TAM - 1);
  destino[OTA_VERSAO_TAM - 1] = '\0';
}

void verificarAtualizacaoOTA() {
  Serial.println(F("[ota] verificando firmware novo no GitHub..."));

  EEPROM.begin(sizeof(EstadoOTA));
  EstadoOTA estado;
  EEPROM.get(0, estado);
  if (estado.magic != OTA_EEPROM_MAGIC) {
    // primeira vez que esta area de EEPROM e usada (ou veio "suja") -
    // comeca do zero, sem nenhuma versao resolvida/em tentativa ainda.
    Serial.println(F("[ota] EEPROM sem estado valido ainda - comecando do zero"));
    estado.magic = OTA_EEPROM_MAGIC;
    estado.versaoResolvida[0]   = '\0';
    estado.versaoEmTentativa[0] = '\0';
    estado.tentativasFalhas     = 0;
  }

  BearSSL::WiFiClientSecure clienteSeguro;
  clienteSeguro.setInsecure();   // sem validar certificado - decisao registrada no doc do projeto

  HTTPClient http;
  String urlVersao = otaVersionUrl();

  if (!http.begin(clienteSeguro, urlVersao)) {
    Serial.println(F("[ota] nao consegui abrir a URL de versao - seguindo sem atualizar"));
    EEPROM.end();
    return;
  }
  http.setTimeout(8000);

  int codigo = http.GET();
  if (codigo != 200) {
    Serial.print(F("[ota] version.txt respondeu HTTP "));
    Serial.print(codigo);
    Serial.println(F(" - seguindo sem atualizar"));
    http.end();
    EEPROM.end();
    return;
  }

  String versaoRemota = http.getString();
  http.end();
  versaoRemota.trim();

  Serial.print(F("[ota] versao publicada no GitHub  : "));
  Serial.println(versaoRemota);
  Serial.print(F("[ota] ultima versao ja resolvida  : "));
  if (estado.versaoResolvida[0]) {
    Serial.println(estado.versaoResolvida);
  } else {
    Serial.println(F("(nenhuma ainda)"));
  }

  if (versaoRemota.length() == 0) {
    Serial.println(F("[ota] version.txt veio vazio - seguindo sem atualizar"));
    EEPROM.end();
    return;
  }

  if (versaoRemota.length() >= OTA_VERSAO_TAM) {
    Serial.println(F("[ota] version.txt maior do que o esperado - ignorando por seguranca"));
    EEPROM.end();
    return;
  }

  // Ja resolvida antes (aplicada com sucesso OU desistida por esgotar
  // tentativas) - nao faz nada, seja qual for o motivo.
  if (versaoRemota == estado.versaoResolvida) {
    Serial.println(F("[ota] ja estou na versao mais recente (ou ja desisti dela) - nada a fazer"));
    EEPROM.end();
    return;
  }

  // E uma versao "pendente" diferente da que estava em tentativa? Reseta o
  // contador - e um alvo novo, merece as tentativas dele do zero.
  if (versaoRemota != estado.versaoEmTentativa) {
    Serial.println(F("[ota] versao nova (nunca tentada) - comecando contagem de tentativas do zero"));
    copiarVersaoPraEstado(estado.versaoEmTentativa, versaoRemota);
    estado.tentativasFalhas = 0;
  }

  Serial.println(F("[ota] baixando e gravando..."));
  Serial.println(F("[ota] NAO desligue a placa agora"));

  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);   // LOW = aceso, mesma convencao do resto do projeto

  // v7: rebootOnUpdate(false) - CORRIGE bug do v6. Confirmado direto na
  // fonte (ESP8266httpUpdate.cpp, core 3.1.2): com rebootOnUpdate(true) a
  // propria biblioteca chama ESP.restart() DE DENTRO do update(), ANTES
  // de devolver o controle pro nosso codigo - ou seja, o EEPROM.put()/
  // commit() que marca a versao como "resolvida" nunca chegava a rodar em
  // caso de sucesso. Com false, update() so retorna HTTP_UPDATE_OK e quem
  // reinicia (so DEPOIS de gravar e confirmar a EEPROM) somos nos mesmos,
  // mais abaixo.
  ESPhttpUpdate.rebootOnUpdate(false);

  t_httpUpdate_return resultado = ESPhttpUpdate.update(clienteSeguro, otaFirmwareUrl());

  switch (resultado) {
    case HTTP_UPDATE_FAILED: {
      estado.tentativasFalhas++;
      Serial.printf("[ota] FALHOU (erro %d): %s\n",
                     ESPhttpUpdate.getLastError(),
                     ESPhttpUpdate.getLastErrorString().c_str());
      Serial.print(F("[ota] tentativa "));
      Serial.print(estado.tentativasFalhas);
      Serial.print(F(" de "));
      Serial.print(OTA_LIMITE_TENTATIVAS);
      Serial.println(F(" pra essa versao"));

      if (estado.tentativasFalhas >= OTA_LIMITE_TENTATIVAS) {
        // TRAVA DE SEGURANCA: esgotou as tentativas - marca como
        // "resolvida" (mesmo sem sucesso) pra PARAR de tentar essa mesma
        // versao pra sempre. So volta a tentar OTA quando o version.txt
        // apontar pra outro valor (ou seja, um firmware.bin novo).
        Serial.println(F("[ota] *** LIMITE ATINGIDO - desistindo desta versao ***"));
        Serial.println(F("[ota] nao vou tentar de novo ate um firmware.bin novo ser publicado"));
        copiarVersaoPraEstado(estado.versaoResolvida, versaoRemota);
      } else {
        Serial.println(F("[ota] vou tentar de novo no proximo boot"));
      }
      Serial.println(F("[ota] continuando com o firmware atual, sem reiniciar"));

      EEPROM.put(0, estado);
      EEPROM.commit();
      break;
    }

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F("[ota] o servidor disse que nao ha atualizacao (nao esperado aqui)"));
      break;

    case HTTP_UPDATE_OK:
      Serial.println(F("[ota] gravado com sucesso! marcando como resolvida..."));
      copiarVersaoPraEstado(estado.versaoResolvida, versaoRemota);
      estado.tentativasFalhas = 0;
      EEPROM.put(0, estado);
      EEPROM.commit();   // GARANTIDO gravado antes do reboot - e por isso que reiniciamos so aqui embaixo
      EEPROM.end();
      Serial.println(F("[ota] reiniciando agora..."));
      delay(200);        // da tempo do Serial esvaziar antes de reiniciar
      ESP.restart();     // agora SOMOS NOS que reiniciamos, so depois do commit confirmado
      break;             // nunca chega aqui de verdade
  }

  EEPROM.end();
}

/* ----------------------- CALLBACK DE RECEPCAO ESP-NOW -------------------- */

void aoReceber(uint8_t *macRemetente, uint8_t *dados, uint8_t tamanho) {
  if (tamanho != sizeof(PacoteChamado)) return;

  uint8_t proxima = (fila_entrada + 1) % FILA_TAM;
  if (proxima == fila_saida) {
    descartados++;
    return;
  }

  PacoteChamado p;
  memcpy(&p, dados, sizeof(p));

  memcpy(fila[fila_entrada].mac, macRemetente, 6);
  fila[fila_entrada].contador = p.contador;
  fila_entrada = proxima;
}

/* ------------------- CALLBACK: PORTAL DE CONFIGURACAO ABRIU --------------- */

void aoAbrirPortal(WiFiManager *wm) {
  Serial.println(F("[portal] modo configuracao ativo"));
  Serial.print(F("[portal] conecte no Wi-Fi \""));
  Serial.print(PORTAL_NOME);
  Serial.println(F("\" para configurar"));
  ledOn();   // fica aceso fixo enquanto o portal estiver esperando voce
}

/* ------------------------- DESCOBRIR O SERVIDOR --------------------------- */

void descobrirServidor() {
  Serial.print(F("[mdns] procurando '_"));
  Serial.print(SERVICO_MDNS);
  Serial.print(F("._"));
  Serial.print(PROTOCOLO_MDNS);
  Serial.print(F(".local.'"));

  for (int tentativa = 0; tentativa < TENTATIVAS_MDNS; tentativa++) {
    int n = MDNS.queryService(SERVICO_MDNS, PROTOCOLO_MDNS);
    if (n > 0) {
      IPAddress ip = MDNS.answerIP(0);
      uint16_t porta = MDNS.answerPort(0);
      url_servidor = "http://" + ip.toString() + ":" + String(porta) + "/chamar";
      achou_por_mdns = true;
      Serial.println(F(" achado!"));
      Serial.print(F("[mdns] servidor: "));
      Serial.println(url_servidor);
      return;
    }
    Serial.print('.');
    delay(500);
  }

  Serial.println(F(" nao achei"));
  url_servidor = String("http://") + SERVIDOR_IP_RESERVA + ":" +
                 SERVIDOR_PORTA_RESERVA + "/chamar";
  achou_por_mdns = false;
  Serial.print(F("[mdns] servidor (reserva): "));
  Serial.println(url_servidor);
}

/* --------------------------------- SETUP ---------------------------------- */

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println();
  Serial.println(F("=============================================="));
  Serial.println(F("   ETAPA 3.1 FINAL v8  -  GATEWAY (neon + OTA, compilado via GitHub Action)"));
  Serial.println(F("=============================================="));
  Serial.print(F("versao deste firmware : "));
  Serial.println(FIRMWARE_VERSION);
  Serial.print(F("motivo do ultimo boot : "));
  Serial.println(ESP.getResetReason());
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);
  ledOff();

  pinMode(PINO_BOTAO_RESET, INPUT_PULLUP);

  /* --- Wi-Fi via WiFiManager, com tema neon v3 e callback do portal --- */

  WiFiManager wm;
  wm.setConnectTimeout(CONEXAO_TIMEOUT_S);
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setTitle("Painel do Garcom");
  wm.setScanDispPerc(true);
  wm.setAPCallback(aoAbrirPortal);
  wm.setCustomHeadElement(PORTAL_CSS);   // sem PROGMEM - ver armadilha 17

  Serial.println(F("[wifi] tentando rede salva (WiFiManager)..."));

  if (!wm.autoConnect(PORTAL_NOME, PORTAL_SENHA)) {
    Serial.println(F("[wifi] portal expirou sem configuracao. Reiniciando..."));
    delay(3000);
    ESP.restart();
  }

  ledOff();  // portal fechou (se tiver aberto) - volta ao normal

  Serial.print(F("[wifi] conectado: "));
  Serial.println(WiFi.SSID());
  Serial.print(F("[wifi] IP do gateway : "));
  Serial.println(WiFi.localIP());

  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Sem alarme de "canal esperado" (v5): o canal do roteador pode mudar
  // (ja mudou 3x nos testes - 1, depois 4, depois 6) e isso deixou de ser
  // um problema desde a Etapa 3.4 - a mesa varre e se adapta sozinha.
  // Aqui so registra qual e o canal atual, pra referencia/depuracao.
  uint8_t canal = wifi_get_channel();
  Serial.print(F("[wifi] canal do roteador agora : "));
  Serial.println(canal);

  /* --- Checagem de OTA: SO AQUI, com Wi-Fi ja estavel, ANTES do          */
  /* mDNS/ESP-NOW nascerem (ver explicacao no cabecalho deste arquivo) --- */

  verificarAtualizacaoOTA();

  if (!MDNS.begin("garcom-gateway")) {
    Serial.println(F("[mdns] ERRO: MDNS.begin() falhou"));
  }

  descobrirServidor();

  if (esp_now_init() != 0) {
    Serial.println(F("[espnow] ERRO: esp_now_init() falhou. Reiniciando..."));
    delay(3000);
    ESP.restart();
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(aoReceber);

  Serial.print(F("[espnow] ouvindo no canal "));
  Serial.println(wifi_get_channel());
  Serial.print(F("[espnow] MAC deste gateway : "));
  Serial.println(WiFi.macAddress());

  Serial.println();
  Serial.print(F("[reset-wifi] segure o botao do D5 por "));
  Serial.print(TEMPO_RESET_MS / 1000);
  Serial.println(F("s para reconfigurar o Wi-Fi"));
  Serial.println();
  Serial.println(F("pronto. aguardando chamados...\n"));
}

/* ---------------------- BOTAO DE RESET DO WI-FI (D5) ----------------------- */
/* Nao-bloqueante: roda a cada volta do loop(), sem delay() longo, para nao
   atrapalhar a recepcao de ESP-NOW nem o processamento da fila.
   Enquanto o botao esta pressionado, o LED pisca cada vez mais rapido -
   uma contagem regressiva visual ate completar os 10 segundos.          */

void verificarBotaoReset() {
  static unsigned long pressionado_desde = 0;
  static unsigned long ultimo_toggle      = 0;
  static bool          led_aceso          = false;

  bool pressionado = (digitalRead(PINO_BOTAO_RESET) == LOW);

  if (!pressionado) {
    if (pressionado_desde != 0) {
      Serial.println(F("[reset-wifi] solto antes dos 10s - cancelado"));
    }
    pressionado_desde = 0;
    if (led_aceso) { ledOff(); led_aceso = false; }
    return;
  }

  // acabou de ser pressionado agora
  if (pressionado_desde == 0) {
    pressionado_desde = millis();
    ultimo_toggle = millis();
    Serial.println(F("[reset-wifi] segurando... solte para cancelar, mantenha"));
    Serial.println(F("[reset-wifi] pressionado por 10s para apagar o Wi-Fi salvo"));
  }

  unsigned long segurando_ms = millis() - pressionado_desde;

  if (segurando_ms >= TEMPO_RESET_MS) {
    Serial.println(F("[reset-wifi] CONFIRMADO - apagando Wi-Fi salvo..."));

    // flash de confirmacao: 6 piscadas bem rapidas
    for (int i = 0; i < 6; i++) {
      ledOn();  delay(50);
      ledOff(); delay(50);
    }

    WiFiManager wmReset;
    wmReset.resetSettings();
    Serial.println(F("[reset-wifi] Wi-Fi apagado. Reiniciando..."));
    delay(300);
    ESP.restart();
  }

  // contagem regressiva visual: intervalo do pisca diminui conforme
  // se aproxima dos 10s (pisca lento no inicio, rapido no final)
  unsigned long intervalo = map(segurando_ms, 0, TEMPO_RESET_MS,
                                 PISCA_LENTO_MS, PISCA_RAPIDO_MS);

  if (millis() - ultimo_toggle >= intervalo) {
    led_aceso = !led_aceso;
    if (led_aceso) ledOn(); else ledOff();
    ultimo_toggle = millis();
  }
}

/* ---------------------------------- LOOP ---------------------------------- */

void loop() {
  MDNS.update();
  verificarBotaoReset();

  if (fila_saida != fila_entrada) {
    ItemFila item;
    memcpy(&item, &fila[fila_saida], sizeof(item));
    fila_saida = (fila_saida + 1) % FILA_TAM;

    recebidos++;

    char mac[18];
    macParaTexto(item.mac, mac);

    Serial.print(F("[rx] chamado de "));
    Serial.print(mac);
    Serial.print(F("  (disparo #"));
    Serial.print(item.contador);
    Serial.println(F(")"));

    enviarPost(mac);
  }

  static unsigned long ultima_tentativa = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - ultima_tentativa > 15000) {
      Serial.println(F("[wifi] caiu - tentando reconectar..."));
      WiFi.reconnect();
      ultima_tentativa = millis();
    }
  }

  if (millis() - ultimo_relatorio > 30000) {
    ultimo_relatorio = millis();
    Serial.print(F("[status] uptime="));
    Serial.print(millis() / 1000);
    Serial.print(F("s  wifi="));
    Serial.print(WiFi.status() == WL_CONNECTED ? F("ok") : F("CAIU"));
    Serial.print(F("  servidor="));
    Serial.print(achou_por_mdns ? F("mdns") : F("reserva"));
    Serial.print(F("  recebidos="));
    Serial.print(recebidos);
    Serial.print(F("  201="));
    Serial.print(ok_201);
    Serial.print(F("  200="));
    Serial.print(ok_200);
    Serial.print(F("  falhas="));
    Serial.print(falhas);
    Serial.print(F("  descartados="));
    Serial.print(descartados);
    Serial.print(F("  heap="));
    Serial.println(ESP.getFreeHeap());
  }

  delay(5);
}

/* ------------------------------ POST HTTP --------------------------------- */

void enviarPost(const char *mac) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[post]   sem Wi-Fi - chamado perdido"));
    falhas++;
    return;
  }

  WiFiClient client;
  HTTPClient http;
  String corpo = String("{\"mac\":\"") + mac + "\"}";

  Serial.print(F("[post]   -> "));

  if (!http.begin(client, url_servidor)) {
    Serial.println(F("URL invalida"));
    falhas++;
    return;
  }

  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  int codigo = http.POST(corpo);

  if (codigo == 201) {
    Serial.println(F("201 - chamado NOVO registrado"));
    ok_201++;
  } else if (codigo == 200) {
    Serial.println(F("200 - ja havia chamado pendente dessa mesa"));
    ok_200++;
  } else if (codigo > 0) {
    Serial.print(F("HTTP "));
    Serial.print(codigo);
    Serial.print(F(" - "));
    Serial.println(http.getString());
    falhas++;
  } else {
    Serial.print(F("FALHA DE CONEXAO: "));
    Serial.println(http.errorToString(codigo));
    falhas++;
  }

  http.end();
}
