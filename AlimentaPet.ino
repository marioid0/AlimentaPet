#include <Servo.h>       // Controla o motor servo
#include <Keypad.h>      // Lê o teclado matricial
#include <Wire.h>        // Comunicação I2C (usado pelo RTC)
#include <RTClib.h>      // Controla o módulo de tempo real (RTC)

// Inicializa o RTC
RTC_DS3231 rtc;

// Define layout e conexões do teclado 4x4
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Servo motor conectado no pino 13
Servo servoMotor;
const int servoPin = 13;

// LEDs para indicar configuração (hora, minuto, ração)
const int ledHoraPin = 10;
const int ledMinutoPin = 11;
const int ledRacaoPin = 12;

// Armazena até 10 horários e porções
const int maxLinhas = 10;
int matriz[maxLinhas][2];  // [horaMinuto, porção]
int totalLinhas = 0;       // Quantas linhas foram preenchidas

// Variáveis de controle de entrada de dados
int horas = 0, minutos = 0, racao = 0;
int digitosHora = 0, digitosMinuto = 0;
bool setHora = false, setMinuto = false, setRacao = false;

// Controle do tempo de ativação do servo
unsigned long servoStartTime = 0;
bool servoActive = false;
int servoDuration = 0;

void setup() {
  Serial.begin(9600);

  // Inicia o módulo de tempo real
  if (!rtc.begin()) {
    Serial.println("Erro ao iniciar o módulo RTC!"); // RTC pode não estar conectado corretamente
  }
  if (rtc.lostPower()) {
    Serial.println("RTC perdeu a hora. Ajuste necessário."); 
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Pode não ser confiável, pois define o horário de compilação
  }

  // Inicia o servo e LEDs
  servoMotor.attach(servoPin);
  servoMotor.write(0);  // Posição inicial (pode variar dependendo do modelo do servo)

  pinMode(ledHoraPin, OUTPUT);
  pinMode(ledMinutoPin, OUTPUT);
  pinMode(ledRacaoPin, OUTPUT);

  Serial.println("Sistema iniciado.");
}

void loop() {
  // Lê uma tecla pressionada
  char tecla = keypad.getKey();

  // Reage conforme a tecla pressionada
  if (tecla) {
    switch (tecla) {
      case 'A': // Começa configuração de novo horário e porção
        iniciarConfiguracao();
        break;

      case 'C': // Confirma e armazena os dados inseridos
        confirmarConfiguracao();
        break;

      case '*': // Reseta todas as variáveis e LEDs
        resetarInformacoes();
        break;

      case '#': // Mostra todos os horários e porções salvos
        mostrarMatriz();
        break;
      
      case 'D': // Gira o servo manualmente (pode ser usado para teste)
        servoMotor.write(90);
        delay(1000); // Uso de delay pode travar o código se repetido em loop
        servoMotor.write(0);
        break;

      default: // Captura números para hora, minuto e porção
        capturarDigito(tecla);
        break;
    }
  }

  verificarHorario(); // Verifica se chegou a hora de distribuir ração
  verificarServo();   // Verifica se o tempo de servo já acabou
}

void iniciarConfiguracao() {
  Serial.println("Setar novo horário:");
  Serial.print("Hora: ");
  setHora = true;
  setMinuto = setRacao = false;
  horas = minutos = racao = 0;
  digitosHora = digitosMinuto = 0;

  // Liga o LED correspondente
  digitalWrite(ledHoraPin, HIGH);
  digitalWrite(ledMinutoPin, LOW);
  digitalWrite(ledRacaoPin, LOW);
}

void confirmarConfiguracao() {
  // Salva a configuração se tudo foi inserido
  if (setRacao && totalLinhas < maxLinhas) {
    matriz[totalLinhas][0] = horas * 100 + minutos; // Compacta hora:minuto num só número
    matriz[totalLinhas][1] = racao;
    totalLinhas++;

    Serial.println("\nNova linha adicionada:");
    Serial.print("Horário: ");
    Serial.print(horas);
    Serial.print(":");
    Serial.print(minutos);
    Serial.print(" | Porção: ");
    Serial.print(racao);
    Serial.println("g");

    resetarConfiguracao();
  } else if (totalLinhas >= maxLinhas) {
    Serial.println("Matriz cheia."); // Limite de 10 horários atingido
  }
}

void resetarInformacoes() {
  // Apaga dados atuais, não apaga matriz salva
  horas = minutos = racao = 0;
  digitosHora = digitosMinuto = 0;
  setHora = setMinuto = setRacao = false;

  Serial.println("Informações resetadas.");

  digitalWrite(ledHoraPin, LOW);
  digitalWrite(ledMinutoPin, LOW);
  digitalWrite(ledRacaoPin, LOW);
}

void resetarConfiguracao() {
  // Parecido com resetarInformações(), pode ser fundido para evitar repetição
  horas = minutos = racao = 0;
  digitosHora = digitosMinuto = 0;
  setHora = setMinuto = setRacao = false;

  digitalWrite(ledHoraPin, LOW);
  digitalWrite(ledMinutoPin, LOW);
  digitalWrite(ledRacaoPin, LOW);
}

void capturarDigito(char tecla) {
  int num = tecla - '0';
  if (tecla >= '0' && tecla <= '9') {
    // Captura dígitos para hora (máximo 2)
    if (setHora && digitosHora < 2) {
      horas = horas * 10 + num;
      digitosHora++;
      Serial.print(num);

      if (digitosHora == 2) {
        setHora = false;
        setMinuto = true;
        Serial.print("\nMinuto: ");
        digitalWrite(ledHoraPin, LOW);
        digitalWrite(ledMinutoPin, HIGH);
      }
    }

    // Captura dígitos para minuto (máximo 2)
    else if (setMinuto && digitosMinuto < 2) {
      minutos = minutos * 10 + num;
      digitosMinuto++;
      Serial.print(num);

      if (digitosMinuto == 2) {
        setMinuto = false;
        setRacao = true;
        Serial.print("\nRação (g): ");
        digitalWrite(ledMinutoPin, LOW);
        digitalWrite(ledRacaoPin, HIGH);
      }
    }

    // Captura qualquer número de dígitos para a ração (sem limite de segurança aqui)
    else if (setRacao) {
      racao = racao * 10 + num;
      Serial.print(num);
    }
  }
}

void mostrarMatriz() {
  Serial.println("Matriz de horários e porções:");
  for (int i = 0; i < totalLinhas; i++) {
    int horaPorcao = matriz[i][0] / 100;
    int minutoPorcao = matriz[i][0] % 100;

    Serial.print("Linha ");
    Serial.print(i + 1);
    Serial.print(": Horário: ");
    Serial.print(horaPorcao);
    Serial.print(":");
    Serial.print(minutoPorcao);
    Serial.print(" | Porção: ");
    Serial.print(matriz[i][1]);
    Serial.println("g");
  }
}

void verificarHorario() {
  DateTime now = rtc.now();
  int horaAtual = now.hour();
  int minutoAtual = now.minute();

  // Verifica se algum horário da matriz bate com o horário atual
  for (int i = 0; i < totalLinhas; i++) {
    int horaPorcao = matriz[i][0] / 100;
    int minutoPorcao = matriz[i][0] % 100;

    if (horaAtual == horaPorcao && minutoAtual == minutoPorcao && !servoActive) {
      servoDuration = matriz[i][1] * 100; // Cuidado: 100ms por grama pode ser longo demais
      servoStartTime = millis();
      servoActive = true;
      servoMotor.write(90); // Aciona o servo
      Serial.print("Porção de ");
      Serial.print(matriz[i][1]);
      Serial.println("g distribuída.");
    }
  }
}

void verificarServo() {
  // Desliga o servo após o tempo definido
  if (servoActive && millis() - servoStartTime >= servoDuration) {
    servoMotor.write(0);
    servoActive = false;
  }
}
