// ================================ BIBLIOTECAS ================================
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "hardware/clocks.h"
#include "pico/bootrom.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

// Bibliotecas personalizadas do projeto
#include "lib/ws2812b.h"
#include "lib/rgb.h"
#include "lib/oledgfx.h"
#include "lib/push_button.h"

// ================================ CONFIGURAÇÕES Wi-Fi ================================
// Credenciais WIFI - Tome cuidado se publicar no github!
#define WIFI_SSID "Nilson"
#define WIFI_PASSWORD "casaesnet"

// ================================ DEFINIÇÕES DE PINOS ================================
// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN   // GPIO do CI CYW43 (LED interno da Pico W)
#define LED_BLUE_PIN 12                 // GPIO12 - LED azul
#define LED_GREEN_PIN 11                // GPIO11 - LED verde
#define LED_RED_PIN 13                  // GPIO13 - LED vermelho

// Configuração do barramento I2C
#define I2C_PORT i2c1

// ================================ CONFIGURAÇÕES DO DISPLAY OLED ================================
/// @brief OLED display pin configuration
#define OLED_SDA 14      ///< Serial data pin (pino de dados serial)
#define OLED_SCL 15      ///< Serial clock pin (pino de clock serial)
#define OLED_ADDR 0x3C   ///< I2C address of OLED (endereço I2C do OLED)
#define OLED_BAUDRATE 400000  ///< I2C communication speed (velocidade de comunicação I2C)

// ================================ MACROS E FUNÇÕES UTILITÁRIAS ================================
// Macro para entrar em modo bootsel (reinicialização por USB)
#define set_bootsel_mode() reset_usb_boot(0, 0)

// ================================ VARIÁVEIS GLOBAIS ================================
// Padrão de bits para acender todas as luzes (5x5 matriz)
static uint8_t all_lights_glyph[] = { 1, 1, 1, 1, 1,
                               1, 1, 1, 1, 1,
                               1, 1, 1, 1, 1,
                               1, 1, 1, 1, 1,
                               1, 1, 1, 1, 1
                            };

// Ponteiros globais para os periféricos (para acesso nas callbacks)
static ws2812b_t *ws_global = NULL;   // Ponteiro global para WS2812B (fita LED)
static rgb_t *rgb_global = NULL;      // Ponteiro global para LEDs RGB
static ssd1306_t *ssd_global = NULL;  // Ponteiro global para display OLED

// ================================ DECLARAÇÕES DE FUNÇÕES ================================
// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// Leitura da temperatura interna do microcontrolador
float temp_read(void);

// Tratamento das requisições do usuário (controle dos dispositivos)
void user_request(char **request);

/**
 * @brief GPIO interrupt callback for button presses
 * @param gpio GPIO pin that triggered interrupt
 * @param event Type of interrupt event
 */
static void gpio_irq_callback(uint gpio, uint32_t event);

// ================================ FUNÇÃO PRINCIPAL ================================
int main()
{
    // ========== INICIALIZAÇÕES BÁSICAS ==========
    stdio_init_all();                        // Inicializa entrada/saída padrão (UART, USB)
    set_sys_clock_khz(128000, false);        // Define clock do sistema para 128MHz

    // ========== DECLARAÇÃO DE ESTRUTURAS LOCAIS ==========
    ws2812b_t ws;    // Estrutura para fita LED WS2812B
    rgb_t rgb;       // Estrutura para LEDs RGB
    ssd1306_t ssd;   // Estrutura para display OLED

    // ========== INICIALIZAÇÃO DOS PERIFÉRICOS ==========
    // Inicializa LEDs RGB (vermelho, verde, azul) com intensidade máxima e PWM de 2048
    rgb_init_all(&rgb, LED_RED_PIN, LED_GREEN_PIN, LED_BLUE_PIN, 1.0f, 2048);
    
    // Inicializa display OLED via I2C
    oledgfx_init_all(&ssd, I2C_PORT, OLED_BAUDRATE, OLED_SDA, OLED_SCL, OLED_ADDR);

    // ========== CONFIGURAÇÃO DE PONTEIROS GLOBAIS ==========
    // Define ponteiros globais para acesso nas funções de callback
    ssd_global = &ssd;
    rgb_global = &rgb;
    ws_global = init_ws2812b(pio0, WS2812B_PIN);  // Inicializa WS2812B no PIO0

    // ========== CONFIGURAÇÃO DE BOTÕES ==========
    pb_config_btn_b();                       // Configura o botão B
    pb_set_irq_callback(&gpio_irq_callback); // Define callback para interrupção do botão
    pb_enable_irq(BUTTON_B);                 // Habilita interrupção do botão B

    // ========== CONFIGURAÇÃO INICIAL DO DISPLAY ==========
    oledgfx_draw_border(&ssd, BORDER_LIGHT);          // Desenha borda no display
    oledgfx_draw_string(&ssd, "HomeControlEvo", 8, 8); // Escreve título
    oledgfx_draw_hline(&ssd, 17, BORDER_LIGHT);       // Desenha linha horizontal
    oledgfx_render(&ssd);                              // Renderiza na tela

    // ========== CONFIGURAÇÃO DO GPIO 16 (BOMBA D'ÁGUA) ==========
    gpio_init(16);           // Inicializa GPIO 16
    gpio_set_dir(16, GPIO_OUT); // Configura como saída
    gpio_put(16, 1);         // Liga a bomba d'água inicialmente

    // ========== INICIALIZAÇÃO DO Wi-Fi ==========
    // Inicializa a arquitetura do cyw43 (chip Wi-Fi)
    while (cyw43_arch_init())
    {
        oledgfx_draw_string(&ssd, "Fail init Wi-Fi\n", 8, 18); // Mostra erro no display
        return EXIT_FAILURE; // Sai do programa em caso de erro
    }

    // GPIO do CI CYW43 em nível baixo (LED interno apagado)
    cyw43_arch_gpio_put(LED_PIN, 0);

    // Ativa o Wi-Fi no modo Station (cliente), permitindo conexões a roteadores
    cyw43_arch_enable_sta_mode();

    // ========== CONEXÃO À REDE Wi-Fi ==========
    // Conectar à rede WiFi - fazer um loop até que esteja conectado
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        oledgfx_draw_string(&ssd, "Fail init Wi-Fi\n", 8, 18); // Mostra erro no display
        return EXIT_FAILURE; // Sai em caso de falha na conexão
    }
    printf("Conectado ao Wi-Fi\n");

    // ========== EXIBIÇÃO DO ENDEREÇO IP ==========
    // Caso seja a interface de rede padrão - imprimir o IP do dispositivo no display
    if (netif_default)
    {
        oledgfx_draw_string(&ssd, "ip addr:", 8, 18);                              // Label do IP
        oledgfx_draw_string(&ssd, ipaddr_ntoa(&netif_default->ip_addr), 8, 32);   // Endereço IP
    }

    // ========== CONFIGURAÇÃO DO SERVIDOR TCP ==========
    // Configura o servidor TCP - cria novos PCBs TCP. É o primeiro passo para estabelecer uma conexão TCP.
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        oledgfx_draw_string(&ssd, "cant creat serv", 8, 40); // Erro na criação do servidor
        return EXIT_FAILURE;
    }

    // Vincula um PCB (Protocol Control Block) TCP a um endereço IP e porta específicos (porta 80 - HTTP)
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        oledgfx_draw_string(&ssd, "cant use 80 port", 8, 40); // Erro ao usar porta 80
        return EXIT_FAILURE;
    }

    // Coloca um PCB (Protocol Control Block) TCP em modo de escuta, permitindo que ele aceite conexões de entrada
    server = tcp_listen(server);

    // Define uma função de callback para aceitar conexões TCP de entrada
    tcp_accept(server, tcp_server_accept);
    oledgfx_draw_string(&ssd, "listn. port 80", 8, 48); // Confirma que está escutando na porta 80

    // ========== INICIALIZAÇÃO DO ADC ==========
    // Inicializa o conversor ADC para leitura da temperatura interna
    adc_init();
    adc_set_temp_sensor_enabled(true); // Habilita sensor de temperatura interno

    // ========== LOOP PRINCIPAL ==========
    while (true)
    {
        cyw43_arch_poll(); // Necessário para manter o Wi-Fi ativo e processar eventos de rede
        sleep_ms(100);     // Reduz o uso da CPU, pausa por 100ms
    }

    // ========== FINALIZAÇÃO (nunca executado devido ao loop infinito) ==========
    cyw43_arch_deinit(); // Desligar a arquitetura CYW43
    return EXIT_SUCCESS;
}

// ================================ IMPLEMENTAÇÃO DAS FUNÇÕES ================================

/**
 * @brief Callback de interrupção do GPIO (botão)
 * @param gpio Pino GPIO que disparou a interrupção
 * @param event Tipo de evento da interrupção
 * 
 * Quando o botão é pressionado, entra em modo bootsel (reinicialização por USB)
 */
static void gpio_irq_callback(uint gpio, uint32_t event) 
{ 
    set_bootsel_mode(); // Reinicia o dispositivo em modo bootloader
}

/**
 * @brief Função de callback ao aceitar conexões TCP
 * @param arg Argumento adicional (não usado)
 * @param newpcb Novo PCB TCP da conexão aceita
 * @param err Código de erro
 * @return Código de erro (ERR_OK se sucesso)
 * 
 * Esta função é chamada toda vez que uma nova conexão TCP é estabelecida
 */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    // Define a função de callback para receber dados desta nova conexão
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

/**
 * @brief Tratamento das requisições do usuário
 * @param request Ponteiro para a string da requisição HTTP
 * 
 * Analisa a URL da requisição e executa a ação correspondente nos dispositivos
 */
void user_request(char **request){

    // ========== CONTROLE DO AR CONDICIONADO ==========
    if (strstr(*request, "GET /ligar_arcondicionado") != NULL)
    {
        rgb_turn_off_red(rgb_global);           // Apaga LED vermelho
        rgb_turn_on_green(rgb_global, 10);      // Liga LED verde (indica ar ligado)
    }
    // ========== CONTROLE DAS LÂMPADAS ==========
    else if (strstr(*request, "GET /ligar_lampadas") != NULL)
    {
        // Liga todas as lâmpadas da fita LED em branco
        ws2812b_draw(ws_global, all_lights_glyph, WHITE, 9);
    }
    // ========== MODO SLEEP DO AR CONDICIONADO ==========
    else if (strstr(*request, "GET /sleep_arcondicionado") != NULL)
    {
        rgb_turn_off_green(rgb_global);         // Apaga LED verde
        rgb_turn_on_red(rgb_global, 10);        // Liga LED vermelho (indica modo sleep)
    }
    // ========== DESLIGAR LÂMPADAS ==========
    else if (strstr(*request, "GET /desligar_lampadas") != NULL)
    {
        ws2812b_turn_off_all(ws_global);        // Apaga todas as lâmpadas da fita LED
    }
    // ========== DESLIGAR BOMBA D'ÁGUA ==========
    else if (strstr(*request, "GET /desligar_bomba") != NULL)
    {
        gpio_put(16, 0);                        // Desliga a bomba d'água (GPIO 16 = LOW)
    }
};

/**
 * @brief Leitura da temperatura interna do microcontrolador
 * @return Temperatura em graus Celsius
 * 
 * Usa o sensor de temperatura interno do RP2040 via ADC
 */
float temp_read(void){
    adc_select_input(4);                                    // Seleciona canal 4 do ADC (sensor de temperatura)
    uint16_t raw_value = adc_read();                        // Lê valor bruto do ADC (0-4095)
    const float conversion_factor = 3.3f / (1 << 12);      // Fator de conversão (3.3V / 4096)
    // Fórmula de conversão específica do RP2040 para temperatura
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;
    return temperature;
}

/**
 * @brief Função de callback para processar requisições HTTP
 * @param arg Argumento adicional (não usado)
 * @param tpcb PCB TCP da conexão
 * @param p Buffer de pacote recebido
 * @param err Código de erro
 * @return Código de erro
 * 
 * Esta é a função principal que processa as requisições HTTP e gera as respostas
 */
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    // ========== VERIFICAÇÃO DE CONEXÃO ==========
    // Se não há dados, fecha a conexão
    if (!p)
    {
        tcp_close(tpcb);        // Fecha a conexão TCP
        tcp_recv(tpcb, NULL);   // Remove o callback de recepção
        return ERR_OK;
    }

    // ========== PROCESSAMENTO DA REQUISIÇÃO ==========
    // Alocação do request na memória dinâmica
    char *request = (char *)malloc(p->len + 1);    // Aloca memória para a requisição
    memcpy(request, p->payload, p->len);           // Copia dados do buffer de rede
    request[p->len] = '\0';                        // Adiciona terminador de string

    printf("Request: %s\n", request);              // Debug: imprime a requisição

    // ========== CONTROLE DOS DISPOSITIVOS ==========
    // Tratamento de request - Controle dos LEDs e outros dispositivos
    user_request(&request);
    
    // ========== LEITURA DE SENSORES ==========
    // Leitura da temperatura interna para exibir no dashboard
    float temperature = temp_read();

    // ========== GERAÇÃO DA RESPOSTA HTML ==========
    // Cria a resposta HTML (buffer grande para a página completa)
    char html[8400];

    // ========== GERAÇÃO DA PÁGINA WEB ==========
    // Instruções HTML do webserver - página completa de automação residencial
    snprintf(html, sizeof(html),
        // ========== CABEÇALHO HTTP ==========
        "HTTP/1.1 200 OK\r\n"                              // Status HTTP OK
        "Content-Type: text/html; charset=UTF-8\r\n"       // Tipo de conteúdo
        "Connection: close\r\n"                             // Fecha conexão após resposta
        "\r\n"                                              // Linha em branco (fim do cabeçalho)
        
        // ========== DOCUMENTO HTML ==========
        "<!DOCTYPE html>\n"
        "<html lang=\"pt\">\n"
        "<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "<title>HomeControl - Automação Residencial</title>\n"
        
        // ========== ESTILOS CSS (RESPONSIVO E MODERNO) ==========
        "<style>\n"
        "* {\n"
        "margin: 0;\n"
        "padding: 0;\n"
        "box-sizing: border-box;\n"         // Box model consistente
        "}\n"
        "body {\n"
        "background: linear-gradient(135deg, #1e1e1e, #333333);\n"  // Gradiente escuro
        "font-family: Arial, sans-serif;\n"
        "color: white;\n"
        "padding: 20px;\n"
        "min-height: 100vh;\n"              // Altura mínima da viewport
        "}\n"
        ".container {\n"
        "max-width: 800px;\n"               // Largura máxima do conteúdo
        "margin: 0 auto;\n"                 // Centraliza horizontalmente
        "}\n"
        "header {\n"
        "text-align: center;\n"
        "margin-bottom: 30px;\n"
        "}\n"
        "h1 {\n"
        "font-size: 2.5rem;\n"
        "background: linear-gradient(90deg, #4361ee, #4cc9f0);\n"   // Gradiente no texto
        "-webkit-background-clip: text;\n"
        "background-clip: text;\n"
        "color: transparent;\n"             // Torna o texto transparente para mostrar o gradiente
        "margin-bottom: 10px;\n"
        "}\n"
        ".subtitle {\n"
        "color: #e0e0e0;\n"
        "font-size: 1.1rem;\n"
        "}\n"
        ".dashboard {\n"
        "display: grid;\n"                  // Layout em grid
        "grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));\n"  // Colunas responsivas
        "gap: 20px;\n"
        "}\n"
        ".card {\n"
        "background: rgba(255, 255, 255, 0.05);\n"  // Fundo semi-transparente
        "border-radius: 12px;\n"
        "padding: 20px;\n"
        "border: 1px solid rgba(255, 255, 255, 0.1);\n"  // Borda sutil
        "}\n"
        ".card-header {\n"
        "display: flex;\n"
        "justify-content: space-between;\n"  // Distribui espaço entre elementos
        "align-items: center;\n"
        "margin-bottom: 20px;\n"
        "padding-bottom: 10px;\n"
        "border-bottom: 1px solid rgba(255, 255, 255, 0.1);\n"
        "}\n"
        ".card-title {\n"
        "font-size: 1.3rem;\n"
        "font-weight: bold;\n"
        "}\n"
        ".status {\n"
        "display: flex;\n"
        "align-items: center;\n"
        "gap: 5px;\n"
        "font-size: 0.9rem;\n"
        "color: #e0e0e0;\n"
        "}\n"
        ".status-dot {\n"
        "width: 10px;\n"
        "height: 10px;\n"
        "background: #4CAF50;\n"             // Verde para indicar online
        "border-radius: 50%;\n"              // Círculo perfeito
        "}\n"
        ".button-grid {\n"
        "display: grid;\n"
        "gap: 12px;\n"
        "}\n"
        ".button {\n"
        "background: rgba(255, 255, 255, 0.08);\n"
        "color: white;\n"
        "border: none;\n"
        "padding: 12px 16px;\n"
        "border-radius: 8px;\n"
        "font-size: 1rem;\n"
        "cursor: pointer;\n"
        "transition: all 0.3s ease;\n"       // Animação suave
        "width: 100%;\n"
        "}\n"
        ".button:hover {\n"
        "background: rgba(255, 255, 255, 0.15);\n"  // Efeito hover
        "transform: translateY(-2px);\n"             // Eleva o botão
        "}\n"
        ".button-primary {\n"
        "background: linear-gradient(145deg, #4361ee, #3a56d4);\n"  // Gradiente azul
        "}\n"
        ".button-danger {\n"
        "background: linear-gradient(145deg, #e63946, #d63031);\n"   // Gradiente vermelho
        "}\n"
        ".metrics {\n"
        "display: grid;\n"
        "grid-template-columns: 1fr 1fr;\n"  // Duas colunas iguais
        "gap: 15px;\n"
        "margin-bottom: 20px;\n"
        "}\n"
        ".metric {\n"
        "background: rgba(255, 255, 255, 0.08);\n"
        "border-radius: 8px;\n"
        "padding: 15px;\n"
        "text-align: center;\n"
        "}\n"
        ".metric-value {\n"
        "font-size: 1.8rem;\n"
        "font-weight: bold;\n"
        "margin: 5px 0;\n"
        "}\n"
        ".metric-label {\n"
        "font-size: 0.9rem;\n"
        "color: #e0e0e0;\n"
        "}\n"
        ".scenes {\n"
        "margin-top: 20px;\n"
        "}\n"
        ".scene-button {\n"
        "background: rgba(67, 97, 238, 0.1);\n"
        "border: 1px solid rgba(67, 97, 238, 0.3);\n"
        "border-radius: 8px;\n"
        "padding: 12px;\n"
        "margin-bottom: 10px;\n"
        "cursor: pointer;\n"
        "transition: all 0.3s ease;\n"
        "display: flex;\n"
        "align-items: center;\n"
        "gap: 10px;\n"
        "}\n"
        ".scene-button:hover {\n"
        "background: rgba(67, 97, 238, 0.2);\n"
        "}\n"
        ".scene-icon {\n"
        "width: 35px;\n"
        "height: 35px;\n"
        "background: #4361ee;\n"
        "border-radius: 50%;\n"              // Ícone circular
        "display: flex;\n"
        "align-items: center;\n"
        "justify-content: center;\n"
        "font-size: 1.1rem;\n"
        "}\n"
        ".scene-title {\n"
        "font-weight: bold;\n"
        "}\n"
        ".scene-description {\n"
        "font-size: 0.85rem;\n"
        "color: #e0e0e0;\n"
        "}\n"
        // ========== RESPONSIVIDADE PARA MOBILE ==========
        "@media (max-width: 600px) {\n"
        ".dashboard {\n"
        "grid-template-columns: 1fr;\n"      // Uma coluna em telas pequenas
        "}\n"
        "h1 {\n"
        "font-size: 2rem;\n"                 // Título menor em mobile
        "}\n"
        ".metrics {\n"
        "grid-template-columns: 1fr;\n"      // Métricas em coluna única
        "}\n"
        "}\n"
        "</style>\n"
        "</head>\n"
        
        // ========== CORPO DA PÁGINA ==========
        "<body>\n"
        "<div class=\"container\">\n"
        
        // ========== CABEÇALHO ==========
        "<header>\n"
        "<h1>🏠 HomeControl</h1>\n"
        "<p class=\"subtitle\">Sua casa inteligente em um só lugar</p>\n"
        "</header>\n"
        
        // ========== PAINEL PRINCIPAL ==========
        "<div class=\"dashboard\">\n"
        
        // ========== CARD DE DISPOSITIVOS ==========
        "<div class=\"card\">\n"
        "<div class=\"card-header\">\n"
        "<h2 class=\"card-title\">🔌 Dispositivos</h2>\n"
        "<div class=\"status\">\n"
        "<div class=\"status-dot\"></div>\n"  // Indicador verde de online
        "<span>Online</span>\n"
        "</div>\n"
        "</div>\n"
        "<div class=\"button-grid\">\n"
        
        // ========== BOTÕES DE CONTROLE ==========
        // Cada botão é um formulário que faz requisição GET para uma URL específica
        "<form action=\"./ligar_arcondicionado\">\n"
        "<button class=\"button button-primary\">\n"
        "❄️ Ligar Ar Condicionado\n"
        "</button>\n"
        "</form>\n"
        "<form action=\"./ligar_lampadas\">\n"
        "<button class=\"button button-primary\">\n"
        "💡 Ligar Lâmpadas\n"
        "</button>\n"
        "</form>\n"
        "<form action=\"./sleep_arcondicionado\">\n"
        "<button class=\"button\">\n"
        "🌙 Sleep para o Ar\n"
        "</button>\n"
        "</form>\n"
        "<form action=\"./desligar_lampadas\">\n"
        "<button class=\"button\">\n"
        "⚡ Desligar Lâmpadas\n"
        "</button>\n"
        "</form>\n"
        "<form action=\"./desligar_bomba\">\n"
        "<button class=\"button button-danger\">\n"
        "🚫 Desligar Bomba d'Água\n"
        "</button>\n"
        "</form>\n"
        "</div>\n"
        "</div>\n"
        
        // ========== CARD DE MONITORAMENTO ==========
        "<div class=\"card\">\n"
        "<div class=\"card-header\">\n"
        "<h2 class=\"card-title\">📊 Monitoramento</h2>\n"
        "<div class=\"status\">\n"
        "<div class=\"status-dot\"></div>\n"
        "<span>Tempo real</span>\n"
        "</div>\n"
        "</div>\n"
        
        // ========== MÉTRICAS ==========
        "<div class=\"metrics\">\n"
        "<div class=\"metric\">\n"
        "<div style=\"color: #ff9f1c; font-size: 1.5rem;\">🌡️</div>\n"
        "<div class=\"metric-value\">%.1f °C</div>\n"  // Aqui será inserida a temperatura real
        "<div class=\"metric-label\">Temperatura</div>\n"
        "</div>\n"
        "<div class=\"metric\">\n"
        "<div style=\"color: #4cc9f0; font-size: 1.5rem;\">💧</div>\n"
        "<div class=\"metric-value\">78 cm</div>\n"     // Valor fixo (poderia ser de um sensor)
        "<div class=\"metric-label\">Nível de Água</div>\n"
        "</div>\n"
        "</div>\n"
        
        // ========== CENAS RÁPIDAS ==========
        "<div class=\"scenes\">\n"
        "<h3 style=\"margin-bottom: 15px; color: #e0e0e0;\">Cenas Rápidas</h3>\n"
        
        // ========== CENA MODO CINEMA ==========
        "<div class=\"scene-button\">\n"
        "<div class=\"scene-icon\">🛋️</div>\n"
        "<div>\n"
        "<div class=\"scene-title\">Modo Cinema</div>\n"
        "<div class=\"scene-description\">Diminui as luzes e liga o ar-condicionado</div>\n"
        "</div>\n"
        "</div>\n"
        
        // ========== CENA MODO NOTURNO ==========
        "<div class=\"scene-button\">\n"
        "<div class=\"scene-icon\">🛏️</div>\n"
        "<div>\n"
        "<div class=\"scene-title\">Modo Noturno</div>\n"
        "<div class=\"scene-description\">Desliga dispositivos e ativa monitoramento</div>\n"
        "</div>\n"
        "</div>\n"
        "</div>\n"     // Fecha div scenes
        "</div>\n"     // Fecha card de monitoramento
        "</div>\n"     // Fecha dashboard
        "</div>\n"     // Fecha container
        "</body>\n"
        "</html>", temperature);  // Substitui %.1f pela temperatura real lida do sensor

    // ========== ENVIO DA RESPOSTA HTTP ==========
    // Escreve dados para envio (mas não os envia imediatamente)
    // TCP_WRITE_FLAG_COPY copia os dados para buffer interno do TCP
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

    // Envia a mensagem efetivamente pela rede
    tcp_output(tpcb);

    // ========== LIMPEZA DE MEMÓRIA ==========
    // Libera memória alocada dinamicamente para a requisição
    free(request);
    
    // Libera um buffer de pacote (pbuf) que foi alocado anteriormente
    pbuf_free(p);

    return ERR_OK; // Retorna sucesso
}