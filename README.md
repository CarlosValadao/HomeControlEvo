# HomeControl - Automação Residencial 🏡 [Vídeo de apresentação](https://youtu.be/rXiZP0baalI)

**HomeControl** é um sistema de automação residencial desenvolvido para a **Raspberry Pi Pico W**. Ele permite controlar dispositivos como ar-condicionado, bomba d'água e lâmpadas por meio de uma interface web acessível de qualquer dispositivo conectado à mesma rede Wi-Fi 🌐. Além disso, o sistema exibe informações como a **temperatura** interna da casa 🌡️ e o **nível de água** do tanque 💧. A plataforma foi projetada para ser eficiente, de fácil acesso e altamente escalável, tornando a automação residencial acessível e funcional.

O projeto foi desenvolvido utilizando **C**, a versão **2.1.0** do **Pico SDK**, a biblioteca **Light Weight IP (lwIP)** para comunicação Wi-Fi e **HTML** e **CSS** para a interface web. O código foi otimizado para ser leve e rápido, garantindo um desempenho sólido em sistemas embarcados com recursos limitados.

## Configuração da Rede Wi-Fi 🔧

Antes de executar qualquer código, configure o **SSID** e a **senha** da rede Wi-Fi nas macros `WIFI_SSID` e `WIFI_PASSWORD` no código fonte. Defina essas variáveis para a rede que você deseja que a **Raspberry Pi Pico W** se conecte.

```#define WIFI_SSID "SuaRedeWiFi"
   #define WIFI_PASSWORD "SuaSenhaWiFi"```

## Funcionalidades ⚙️

### 1. **Ar-Condicionado (Simulado por LED RGB) 🌈**

   O estado do ar-condicionado é representado por um **LED RGB**:

- **Vermelho**: Ar-condicionado **desligado** ❌.
- **Verde**: Ar-condicionado **ligado** ✅.

   O usuário pode controlar o ar-condicionado através da interface web, alterando a cor do LED.

### 2. **Bomba d'Água (Simulada por Servo Motor) 🔄**

   A bomba d'água é simulada por um **servo motor**:

- Quando ligado, o servo motor **gira**, indicando que a bomba está funcionando 🔄.
- Quando desligado, o servo motor permanece **parado** 🚫.

   A interface web permite ao usuário controlar o estado da bomba, acionando o servo motor conforme necessário.

### 3. **Lâmpadas (Simuladas por Matriz de LEDs 5x5 WS2812B) 💡**

   As lâmpadas são simuladas por uma **matriz de LEDs 5x5 (WS2812B)**:

- **Desligadas**: A matriz de LEDs fica **apagada** 📴.
- **Ligadas**: A matriz de LEDs acende na cor **branca** ⚪.

   A interface web permite ao usuário ligar ou desligar as lâmpadas, controlando os LEDs na matriz.

### 4. **Temperatura Interna da Casa 🌡️**

   A **temperatura interna** da casa é obtida a partir do **sensor de temperatura** da **Raspberry Pi Pico W**. Essa informação é exibida na interface web, permitindo ao usuário monitorar a temperatura em tempo real.

### 5. **Nível de Água do Tanque 💧**

   O **nível de água** do tanque é monitorado e exibido na interface web, permitindo que o usuário acompanhe o status do tanque e da bomba d'água.

### 6. **Exibição do IP da Placa (Display OLED) 📱**

O **display OLED** exibe:

- A **temperatura** interna da casa 🌡️.
- O **nível de água** do tanque 💧.
- O **IP fornecido pelo DHCP** à **Raspberry Pi Pico W**, permitindo que o usuário veja rapidamente o endereço IP da placa e facilite o acesso ao servidor web 🌐.

## Arquitetura do Sistema 🏗️

### 1. **Webserver 🌍**

O sistema é executado em um **webserver** na **Raspberry Pi Pico W**, que é acessado via Wi-Fi. A interface web foi desenvolvida usando **HTML** e **CSS**, permitindo que o usuário controle remotamente o ar-condicionado, a bomba d'água e as lâmpadas. A interface é simples e intuitiva, fornecendo uma forma acessível para o controle dos dispositivos da casa.

### 2. **Módulo Wi-Fi (CYW43439) 🌐**

A **Raspberry Pi Pico W** utiliza o módulo **CYW43439**, que fornece conectividade **Wi-Fi** e **Bluetooth 5.2**. Esse módulo permite que a **Pico W** se conecte à rede sem fio e atue como servidor web, possibilitando o controle remoto dos dispositivos da casa. A comunicação Wi-Fi é gerida pela biblioteca **Light Weight IP (lwIP)**, que oferece uma solução leve e eficiente para redes TCP/IP.

### 3. **Periféricos Utilizados 🛠️**

- **LED RGB**: Simula o controle do ar-condicionado. A cor do LED indica se o dispositivo está ligado ou desligado.
- **Matriz de LEDs (WS2812B)**: Simula o controle das lâmpadas da casa, acendendo em **branco** quando as lâmpadas estão ligadas.
- **Servo Motor**: Simula a bomba d'água, girando quando a bomba é ligada e parando quando desligada.
- **Display OLED**: Exibe informações como a **temperatura interna** e o **nível de água** do tanque, além do **IP** da **Pico W**.

## Como Rodar o Sistema 🚀

### Requisitos 📋

- **Raspberry Pi Pico W** (ou placa compatível com RP2040)
- **Pico SDK** instalado
- **FreeRTOS** configurado
- **Display OLED** com interface **I2C**
- **Matriz de LEDs WS2812B**
- **Servo motor**
- **LED RGB**
- **Módulo Wi-Fi (CYW43439)** integrado

### Passo a Passo de Instalação 🛠️

1. **Clone o repositório**:

   ```bash
   git clone https://github.com/seuusuario/HomeControl.git
   cd HomeControl
   mkdir build
   cd build
   cmake ..
   make```
