/*
 AUTOR: JOAO VITOR B.S. MERENDA
 ORIENTADOR: ODEMIR MARTINEZ BRUNO
 PROJETO: AUTOMAÇÃO DE CÂMARAS DE CRESCIMENTO DE PLANTAS
--------------------------------------------------------------
*/
//Bibliotecas
#include <Nextion.h>
#include <DS3231.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_TSL2561_U.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Ethernet.h>
#include <mq_sensor.h>
//-------------------------------------------
//Definindo os elementos das telas
//-------------------------------------------
NexText txt_date = NexText(0, 3, "date");
NexText txt_hour = NexText(0, 4, "hour");
NexProgressBar j0 = NexProgressBar(1, 8, "j0");
NexText txt_temp = NexText(1, 3, "temp");
NexText txt_umid = NexText(1, 6, "umid");
NexText txt_ilum = NexText(2, 2, "lux");
//NexText txt_CO2 = NexText(2, 5, "co2");
NexText txt_PPFD = NexText(3, 4, "ppfd");
NexText txt_DLI1 = NexText(3, 6, "DLI1");
NexText txt_DLI2 = NexText(3, 8, "DLI2"); 
NexDSButton bt_lamp = NexDSButton(4, 4, "btlamp");
NexDSButton bt_mode = NexDSButton(4, 5, "btmode");
NexText txt_Lamp = NexText(4, 8, "lamp");
NexText txt_Mode = NexText(4, 9, "mode");
NexButton dec_on = NexButton(5, 4, "on_menos");
NexButton dec_off = NexButton(5, 5, "off_menos");
NexButton inc_on = NexButton(5, 6, "on_mais");
NexButton inc_off = NexButton(5, 7, "off_mais");
NexText txt_on = NexText(5, 8, "on");
NexText txt_off = NexText(5, 9, "off");

//----------------------------------------
//Configuracao dos sensores
//----------------------------------------

//Definicoes do RTC DS3231 e do DHT11
DS3231  rtc(SDA, SCL);   //clock
#define DHTPIN A1      // pino que estamos conectado
#define DHTTYPE DHT11 // DHT 11

//Sensor TSL2561
Adafruit_TSL2561_Unified tsl=Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT,12345);
void configureSensor(void)
{
    tsl.enableAutoRange(true);         
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);     
   
}

/*---------------DADOS DA REDE----------*/
// Complete com as informacoes da sua rede
byte mac[] =
{
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

//IPAddress gateway(170, 83, 121, 16);
//IPAddress dns(8, 8, 4, 4);
IPAddress ip(192, 168, 0, 184);
//IPAddress ip(192, 168, 0, 184);
// Inicializa o servidor Ethernet com o IP e a porta 84
EthernetServer server(80);
//---------------------------------------

//----------------------------------------
//Definindo as variaveis globais
//-----------------------------------------
Time t;
String readString;
DHT dht(DHTPIN, DHTTYPE);
bool Lampada = 0;        //ON(1)/OFF(0)
bool Modo = 0;           //MANUAL(0)/AUTO(1)
int on = 0;             //INICIO DO FOTOPERIODO
int off = 10;           //FIM DO FOTOPERIODO
int led = 12;
uint32_t dual_state_lamp;
uint32_t dual_state_mode;
File meuArquivo1;           
File meuArquivo2;
int CS_PIN = 4;          //pino CS do SD
int saidaSensor = 5;
String data;
int task;
float PPFD;
float DLI1;
float DLI2;
long trigger;
int out_pin = 6;

NexTouch *nex_listen_list[] = 
{
    &dec_on, 
    &dec_off, 
    &inc_on, 
    &inc_off,
    NULL
};   

//----------------------------------------
//Funcao SETUP
//----------------------------------------
void setup() {
  //inicializa dispositivos e sensores
    Serial.begin(9600);
    nexInit();
    rtc.begin();
    dht.begin();
    SD.begin(CS_PIN);
    Ethernet.begin(mac, ip);
    server.begin();
    mq.begin(trigger, out_pin, NULL);
    pinMode(led, OUTPUT);
    pinMode(saidaSensor, OUTPUT);
    pinMode(CS_PIN, OUTPUT);
    //adiciona o valor do evento no botao
    dec_on.attachPush(dec_onPopCallback);
    dec_off.attachPush(dec_offPopCallback);
    inc_on.attachPush(inc_onPopCallback);
    inc_off.attachPush(inc_offPopCallback);
    dbSerialPrintln("setup done");
    
}

//-------------------------------------
//Funcao LOOP
//-------------------------------------
void loop() {
  //Obtem dados dos Sensores
  float umid = dht.readHumidity();     //dht humidade
  float temp = dht.readTemperature();   //dht temperatra
  float event_co2 = mq.compare(trigger, out_pin); //inicia um evento no sensor MQ
  float co2 = mq.measure(event_co2);       //mede a concentracao de CO2
  nexLoop(nex_listen_list);            //Ouve os botoes da tela
  t = rtc.getTime();                   //horario hh.mm.ss
  sensors_event_t event;              //TSL-iluminancia
  tsl.getEvent(&event);
  int valor = event.light;
  data = rtc.getDateStr();           //data dd.mm.aaaa
  //na data muda . para /
  for(int i = 0; i < 16; i++){    
    if(data[i] == '.'){
    data[i] = '/';
    }
  }
  //chamada das funcoes  
  //funcoes "Botoes"
  Modo = mode_button();           
  Lampada = lamp_button();        
  //Chama as funcoes de aplicacoes
  photoflux(valor, on, off);
  bool status_lamp = lamp_control();
  exibe_tela(temp, umid, valor, co2);
  escreve_arquivo(temp, umid, valor, status_lamp, co2);
  servidor(temp, umid, valor, status_lamp, co2);
    
  }

//Funcoes para os botoes da tela
//Funcoes para verificar se o botao foi acionado
//1)  - on (decrementa valor de on)
void dec_onPopCallback(void *ptr){
  if(on > 0){
    on = on - 1;   //diminui o valor de on
  }
}

//2) - off (decrementa valor de off)
void dec_offPopCallback(void *ptr){
  if(off > 0){
    off = off - 1;  //diminui o valor de off
  }
}

//3) + on (incrementa valor de on)
void inc_onPopCallback(void *ptr){
  if(on < 23 ){
    on = on + 1;  //incrementa o valor de on
  }
}

//4) + off  (incrementa valor de off)
void inc_offPopCallback(void *ptr){
  if(off < 23){
    off = off + 1;  //incrementa o valor de off
  }
  
}

//5) DUal state button mode
bool mode_button(){
   bool mode_status;
   bt_mode.getValue(&dual_state_mode);  //verifica qual o estado do botao
   //se for 1 --> Auto; se for 0 --> Manual
   if((dual_state_mode == 1) && (dual_state_mode != 0)){
    mode_status = 1;
    bt_mode.setValue(1);
    txt_Mode.setText("Auto");
  }
  if((dual_state_mode == 0) && (dual_state_mode != 1)){
    mode_status = 0;
    bt_mode.setValue(0);
    txt_Mode.setText("Manual");
  }
  return mode_status;
}

//6) dual state button lampada
bool lamp_button(){
  bool lamp_button_status;
  bt_lamp.getValue(&dual_state_lamp);   //verifica o valor do estado do botao
  //se for 0 --> lampada nao acionada, se for 0 --> lampada acionada
  if((dual_state_lamp == 1) && (dual_state_lamp != 0)){
    lamp_button_status = 1;
    bt_lamp.setValue(1);
  }
  if((dual_state_lamp == 0) && (dual_state_lamp != 1)){
    lamp_button_status = 0;
    bt_lamp.setValue(0);
  }
  return lamp_button_status;
}
  
//------------------------------------------------
//Funcoes de aplicacao
//----------------------------------------------

//Funcao app 1 - exibe dados dos sensores na tela
void exibe_tela(float temp, float umid, int valor, float co2){
   //variaveis locais
   char temperature[6];
   char humidity[6];
   char lux[6];
   char on_string[6];
   char off_string[6];
   char co2_string[6];
   int temp_perc = 0;
   temp_perc = (temp/0.5);     //transforma tem em %
   //insere texto com os dados dos sensores
   txt_date.setText(rtc.getDateStr());
   txt_hour.setText(rtc.getTimeStr());
   j0.setValue(temp_perc);
   utoa(temp, temperature, 10);
   utoa(umid, humidity, 10);
   utoa(valor, lux, 10);
   utoa(on, on_string, 10);
   utoa(off, off_string, 10);
   utoa(co2, co2_string, 10);
   txt_temp.setText(temperature);
   txt_umid.setText(humidity);
   txt_ilum.setText(lux);
   txt_on.setText(on_string);
   txt_off.setText(off_string);
   txt_co2.setText(co2_string);   
}

//funcao app 2 - controle da lampada
bool lamp_control(){
  bool status_lamp;
  //O status_lamp é uma combinacao dos valores de Modo e Lampada
  //modo & lampada = status_lam: 
  //0&0 = 0
  //0&1 = 1
  //1&0 (depende do tempo
  //1&1 (depende do tempo
  
  //modo = manual
  if(Modo == 0){
    //lampada = off
    if(Lampada == 0){
      digitalWrite(led, LOW);
      status_lamp = 0;
      txt_Lamp.setText("OFF");
    }
    else{
      //lampada = on
      digitalWrite(led, HIGH);
      status_lamp = 1;
      txt_Lamp.setText("ON");
    }
    }
   //modo = auto (fotoperiodo) 
   if(Modo == 1){
    //implementa o fotoperiodo
    if(on < off){
      if((t.hour >= on) && (t.hour < off)){
        digitalWrite(led, HIGH);
        status_lamp = 1;
        txt_Lamp.setText("ON"); 
      }
      else{
        digitalWrite(led, LOW);
        status_lamp = 0;
        txt_Lamp.setText("OFF");
      }
    }
    if(on > off){
      if((t.hour >= off) && (t.hour < on)){
        digitalWrite(led, LOW);
        status_lamp = 0;
        txt_Lamp.setText("OFF");
      }
      else{
        digitalWrite(led, HIGH);
        status_lamp = 1;
        txt_Lamp.setText("ON");
      }
    }
    if(on == off){
      digitalWrite(led, LOW);
      status_lamp = 0;
      txt_Lamp.setText("OFF");
    }
   }
  return status_lamp;
}

//Funcao app 3 - escreve os dados no cartao de memoria
void escreve_arquivo(float temperatura, float h, int valor, bool status_lamp, float co2){
    //A cada minuto imprime no arquivo os dados dos sensores
    //e escreve em outro arquivo o status da lampada e do modo
  if(task != t.min){
  
    hour_format();
    //Abre meuArquivo1 (dados dos sensores)
    if (meuArquivo1 = SD.open("Data.txt", FILE_WRITE)) //abre o arquivo SD 
  {
    
      meuArquivo1.print("   ");
      meuArquivo1.print(temperatura);
      meuArquivo1.print("   ");
      meuArquivo1.print(h);       //umidade
      meuArquivo1.print("   ");
      meuArquivo1.print(co2);
      meuArquivo1.print("   ");
      meuArquivo1.print(valor);
      meuArquivo1.print("   ");
      meuArquivo1.print(PPFD);
      meuArquivo1.print("   ");
      meuArquivo1.println(DLI1);
      meuArquivo1.close();
   }
    //Abre o meuArquivo2 (arquivo de log)
    if (meuArquivo2 = SD.open("log.txt", FILE_WRITE)) //abre o arquivo SD 
  {
  
      if(Modo == 0){
        meuArquivo2.print("   ");
        meuArquivo2.print("-");
        meuArquivo2.print("   ");
        meuArquivo2.print("-");
        meuArquivo2.print("   ");
        meuArquivo2.print("Manual");
       }
      if(Modo == 1){
        meuArquivo2.print("   ");
        meuArquivo2.print(on);
        meuArquivo2.print("   ");
        meuArquivo2.print(off);
        meuArquivo2.print("   ");
        meuArquivo2.print("Auto");
      }
      meuArquivo2.print("   ");
      if(status_lamp == 0){
        meuArquivo2.println("OFF");
      }
      if(status_lamp == 1){
        meuArquivo2.println("ON");
      }
      meuArquivo2.close();
   }
   task = t.min;
 }
  
}


//Funcao app 4 - calculo dos valores de PPFD e LDI

void photoflux(int valor, int on, int off){
  char PPFD_string[4];
  char DLI1_string[4];
  char DLI2_string[4];
  float photoperiod = (abs(off - on))/24.0;
  PPFD = 0.03*valor;
  DLI1 = 0.0036*PPFD*photoperiod;
  DLI2 = 0.0036*PPFD;
  dtostrf(PPFD, 7, 3, PPFD_string);
  //ltoa(PPFD, PPFD_string, 10);
  txt_PPFD.setText(PPFD_string);
  dtostrf(DLI1, 5, 3, DLI1_string);
  txt_DLI1.setText(DLI1_string);
  dtostrf(DLI2, 5, 3, DLI2_string);
  txt_DLI2.setText(DLI2_string);
  
}


//Funcao app 5 - implementacao do servidor
void servidor(float temp, float umid, int valor, bool status_lamp, float co2){
  EthernetClient client = server.available();
  if (client){
    //variaveis locais
    bool indtxtDataSheet = false;    
    bool indtxt2DataSheet = false;
    bool currentLineIsBlank = true;
    char linebuf[80];
    memset(linebuf, 0, sizeof(linebuf));

   //se o cliente se conectou:
   if (client.connected())
   {
   if (client.available())
   {
    //le a string de requisição (HTTP)
   char c = client.read();
   if (readString.length() < 100)
   {
   //armazena o valor lido na string
   readString += c;
   //copia a mensagem para linebuf
   for(int j = 0; j<readString.length();j++){ 
   linebuf[j] = readString[j];
   }
   }
   //verifica se uma mensagem de requisição foi enviada pelo cliente
   if (c == '\n') 
   {
    //Se a requisção foi para baixar um arquivo de log:    
    if(currentLineIsBlank){
      //Se o cliente clicou no link para baixar Data.txt
      if (strstr(linebuf, "GET /DATA.TXT")) { indtxtDataSheet = true; }
      memset(linebuf, 0, sizeof(linebuf));
      //charCount = 0;
      if(indtxtDataSheet){
         txt_file_download(client, "DATA.TXT");        //datasheet download
      } 
      //Se o cliente clicou no link para baixar log.txt
      if (strstr(linebuf, "GET /LOG.TXT")>0 ) {indtxt2DataSheet = true; }
      if(indtxt2DataSheet){
              txt_file_download(client, "LOG.TXT");
            }
 
    }
   
   //Requisição: Página HTML.
   client.println("HTTP/1.1 200 OK");
   client.println("Content-Type: text/html");
   client.println("Refresh: 120");
   client.println();
   //codigo HTML
   //Pagina web separada em diversas secoes
   client.println("<HTML>");
   client.println("<HEAD>");
   client.println("<TITLE>SERVIDOR BOD</TITLE>");
   client.println("<meta charset='utf-8'>");
   client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
   client.println("<style>");
   estilo(client); 
   client.println("</style>");
   client.println("</HEAD>");
   client.println("<BODY>");
   client.println("<section>");
   //secao cabecalho
   client.println("<header>");
   head(client);
   client.println("</header>");
   //secao sensores
   client.println("<sensor>");
   sensores(client, temp, umid, valor, co2);
   client.println("</sensor>");
   //secao controle de fotoperiodo e iluminacao
   client.println("<controle>");
   control(client);
   client.println("</controle>");
   //secao status da iluminacao
   client.println("<ilum>");
   statusilum(client, status_lamp);
   client.println("</ilum>");
   ////secao dados acerca da lampada (espectro, potencia, etc)
   client.println("<photo>");
   lampada(client);
   client.println("</photo>");
   client.println("</section>");
   client.println("</body>");
   client.println("</html>"); 
   client.stop();

}

  
   }
   }
  }
}

//---------------------------------------------
//Funcoes Auxiliares
//--------------------------------------------


//Funcao Auxiliar 1 - modifica o formato de exibicao do horario no arquivo
void hour_format(){
  //Abre meuArquivo1 e meuArquivo2
  if ((meuArquivo1 = SD.open("Data.txt", FILE_WRITE)) && (meuArquivo2 = SD.open("log.txt", FILE_WRITE))) //abre o arquivo SD 
  {
  //Quando for 00:00 (meia noite) imprime uma separacao de dia
  if(t.hour == 0 && t.min == 0){
    meuArquivo1.println("--------------------------------------------------------------");
    meuArquivo1.print("                      ");
    meuArquivo1.println(data);
    meuArquivo1.println(" ");
    meuArquivo1.println("--------------------------------------------------------------");
    meuArquivo1.print("DATA");
    meuArquivo1.print("        ");
    meuArquivo1.print("HORARIO");
    meuArquivo1.print("   ");
    meuArquivo1.print("TEMP");
    meuArquivo1.print("   ");
    meuArquivo1.print("UMID");
    meuArquivo1.print("   ");
    meuArquivo1.print("[CO2]");
    meuArquivo1.print("   ");
    meuArquivo1.print("ILUM");
    meuArquivo1.print("   ");
    meuArquivo1.print("PPFD");
    meuArquivo1.print("   ");
    meuArquivo1.println("DLI");
    meuArquivo1.println("--------------------------------------------------------------");
    meuArquivo2.println("--------------------------------------------------------------");
    meuArquivo2.print("                      ");
    meuArquivo2.println(data);
    meuArquivo2.println(" ");
    meuArquivo2.println("--------------------------------------------------------------");
    meuArquivo2.print("DATA");
    meuArquivo2.print("       ");
    meuArquivo2.print("HORARIO");
    meuArquivo2.print("  ");
    meuArquivo2.print("ON");
    meuArquivo2.print("  ");
    meuArquivo2.print("OFF");
    meuArquivo2.print("  ");
    meuArquivo2.print("MODO");
    meuArquivo2.print("  ");
    meuArquivo2.println("LAMPADA");
    meuArquivo2.println("--------------------------------------------------------------");
  }
  
  
  meuArquivo1.print(rtc.getDateStr());   //imprime data '.'
  meuArquivo1.print("   "); 
  meuArquivo2.print(rtc.getDateStr());   //imprime data '.'
  meuArquivo2.print("   ");
  //formatacao da hora 
  //se hora for menor que 10 coloca zero á esquerda
  if(t.hour <10){
    meuArquivo1.print("0");
    meuArquivo1.print(t.hour);
    meuArquivo2.print("0");
    meuArquivo2.print(t.hour);
  }
  else{
  meuArquivo1.print(t.hour);
  meuArquivo2.print(t.hour);
  }
  
  meuArquivo1.print(".");  //coloca '.' entre hora e minuto
  meuArquivo2.print(".");
  //Se o minuto for menor que 10, coloca zero á esquerda
   if(t.min < 10){
    meuArquivo1.print("0");
    meuArquivo1.print(t.min);
    meuArquivo2.print("0");
    meuArquivo2.print(t.min);
  }
  else{
  meuArquivo1.print(t.min);
  meuArquivo2.print(t.min);
  }
  meuArquivo1.close();
  meuArquivo2.close();
  }
}

//Funcao auxiliar 2 - estilo da pagina HTML
void estilo(EthernetClient client_aux){
  client_aux.println("body {");
   client_aux.println("font-family: Arial, Helvetica, sans-serif;");
   client_aux.println("font-size: 18px;}");

   client_aux.println("header {");
   client_aux.println("background-color: #FFF;");
   client_aux.println("border: 1px solid black;");  
   client_aux.println("text-align: left;");
   client_aux.println("font-size: 15px;");
   client_aux.println("color: black;}");

   client_aux.println("sensor {");
   client_aux.println("float: left;");
   client_aux.println("width: 50%;");
   client_aux.println("height: 300px;"); 
   client_aux.println("border: 1px solid black;");
   client_aux.println("background: #FFF;}");
  
   client_aux.println("controle {");
   client_aux.println("float: right;");
   client_aux.println("width: 49%;");
   client_aux.println("background-color: #FFF;");
   client_aux.println("height: 522px;"); 
   client_aux.println("border: 1px solid black;");
   client_aux.println("font-size: 15px;}");


   client_aux.println("ilum {");
   client_aux.println("float: left;");
   client_aux.println("border: 1px solid black;");
   client_aux.println("width: 50%;");
   client_aux.println("background-color: #FFF;");
   client_aux.println("height: 220px;}");
    
    
   client_aux.println("photo {");
   client_aux.println("float: left;");
   client_aux.println("border: 1px solid black;");
   client_aux.println("width: 100%;");
   client_aux.println("background-color: #FFF;");
   client_aux.println("height: 1000px;}");

   client_aux.println("section:after {");
   client_aux.println("content: '';");
   client_aux.println("display: table;");
   client_aux.println("clear: both;}");

client_aux.println("@media (max-width: 600px) {");
  client_aux.println("sensor, controle {");
    client_aux.println("width: 100%;");
    client_aux.println("height: auto;}");
    client_aux.println("}");
}

//Funcao Auxiliar 3 - secao cabecalho
void head(EthernetClient client_aux){
   client_aux.println("<h1>Sistema de automação de câmaras de crescimento de plantas</h1>");
   client_aux.println("<h3>Projeto: João Vitor B.S. Merenda</h3>");
   client_aux.println("<h3>Orientador: Odemir Martinez Bruno</h3>");
   client_aux.println("<h3>Instituto de Física de São Carlos - USP</h3>");
   client_aux.println("<h3>");
   client_aux.println(rtc.getDateStr());
   client_aux.println("</h3>");
   client_aux.println("<hr>");
}


//Funcao Auxiliar 4
//Função que exibe os dados dos sensores
void sensores(EthernetClient client_aux, float temp, float umid, int valor, float co2)
{
    client_aux.println("<h1><center>Sensores</center></h1>");
    client_aux.println("<p>Temperatura: ");
    client_aux.println(temp);
    client_aux.println("oC");
    client_aux.println("</p>");
    client_aux.println("<p>Umidade: ");
    client_aux.println(umid);
    client_aux.println("%");
    client_aux.println("</p>");
    client_aux.println("<p>Iluminância: ");
    client_aux.println(valor);
    client_aux.println("lux");
    client_aux.println("</p>"); 
  
    client_aux.println("<p>Concentração de CO2: ");
    client_aux.println(co2);
    client_aux.println("ppm");
    client_aux.println("</p>"); 
    client_aux.println("<p><strong>Ultima atualização: </strong>");
    client_aux.println(rtc.getTimeStr());
    client_aux.println("</p>");
    
}

//Funcao Auxiliar 5 - status da iluminacao
void statusilum(EthernetClient client_aux, bool status_lamp){
    client_aux.println("<h1><center>Status da Iluminação</center></h1>");
    client_aux.println("<p>Lâmpada: ");
    if(status_lamp == 0){
    client_aux.println("OFF");
    }
    else{
      client_aux.println("ON");
    }
    client_aux.println("</p>");
    if(Modo == 0){
    client_aux.println("<p>Modo: ");
    client_aux.println("Manual");
    client_aux.println("</p>");
    client_aux.println("<p>Fotoperíodo: [");
    client_aux.println("-");
    client_aux.println(",");
    client_aux.println("-");
    client_aux.println("]h</p>");
    }   
    else{
    client_aux.println("<p>Modo: ");
    client_aux.println("Auto");
    client_aux.println("</p>");
    client_aux.println("<p>Fotoperíodo: [");
    client_aux.println(on);
    client_aux.println(",");
    client_aux.println(off);
    client_aux.println("]h</p>");
    } 
}

//Funcao Auxiliar 6 - Controle de iluminacao
void control(EthernetClient client_aux){
    client_aux.println("<h1><center>Controle de Iluminação</center></h1>");
    client_aux.println("<form method=post>");
    client_aux.println("<label for='pwd'>Password:</label><br>");
    client_aux.println("<input type='password' id='pwd' name='pwd'>");
    client_aux.println("<input type='submit' value='Submit'><br><br>");
    client_aux.println("<label for='bt1'>Modo:</label><br>");
    client_aux.println("<input type='button' value='Auto' id='bt1' name='bt1'>");
    client_aux.println("<input type='button' value='Manual' id='bt2' name='bt2'><br><br>");
    client_aux.println("<label for='bt3'>Lâmpada:</label><br>");
    client_aux.println("<input type='button' value='Desligar' id='bt3' name='bt3'>");
    client_aux.println("<input type='button' value='Ligar' id='bt4' name='bt4'><br><br>");
    client_aux.println("<label for='quantity'>Início do Fotoperíodo:</label><br>");
    client_aux.println("<input type='number' id='quantity' name='quantity' min='0' max='23' step='1' value='0'>");
    client_aux.println("<input type='submit' value='Submit'><br><br>");
    client_aux.println("<label for='quantity2'>Fim do Fotoperíodo:</label><br>");
    client_aux.println("<input type='number' id='quantity2' name='quantity2' min='0' max='23' step='1' value='0'>");
    client_aux.println("<input type='submit' value='Submit'><br><br>");
    client_aux.println("</form>");
    client_aux.println("<ul>");
    client_aux.println("<li>Para conseguir controlar a iluminação da câmara é necessário inserir uma palavra-chave.</li>");
    client_aux.println("<li>Ao ativar o modo 'Automático' o usuário deverá definir um fotoperíodo. Ligar e desligar a lâmpada manualmente não será mais possível nesse modo.</li>");
    client_aux.println("<li>Ao ativar o modo 'Manual' o usuário poderá ligar e desligar a lâmpada a qualquer horário, mas não conseguirá definir um fotoperíodo.</li>");
    client_aux.println("<li>Os horários do fotoperíodo seguem o padrão de 24h, indo do valor de 0 à 23.</li>");
    client_aux.println("</ul>");
}

//Funcao Auxiliar 7 - dados da lampada
void lampada(EthernetClient client_aux){
     client_aux.println("<h1>Características da Lâmpada</h1>");
     client_aux.println("<p>Potencia: 100W </p>");
     client_aux.println("<p>Distribuição de LEDs por cor (Normalizado): 0.69R + 0.24B + 0.069W </p>");
     client_aux.println("<p>Espectro do branco: 3500k = 0.26R + 0.59G + 0.15B</p>");
     client_aux.println("<p>Espectro da Lampada: 0.692R + 0.06G +0.26B </p>");
     client_aux.println("<p>Photosynthetic Efficacy: 2.5 umol/J</p>");
     client_aux.println("<p>Beam Angle: 120</p>");
     client_aux.println("<p><strong>Legenda:</strong> R = vermelho, G = Verde, B = Azul, W = Branco. </p>");
     client_aux.println("<p><strong> Photosynthetic Photon flux:</strong></p>");
     client_aux.println("<p style='color:Tomato;'>");
     client_aux.println("PPFD: ");
     client_aux.println(PPFD);
     client_aux.println("</p>");
     client_aux.println("<p style='color:Tomato;'>");
     client_aux.println("DLI*: ");
     client_aux.println(DLI1);
     client_aux.println("</p>");
     client_aux.println("<p style='color:Tomato;'>");
     client_aux.println("DLI**: ");
     client_aux.println(DLI2);
     client_aux.println("</p>");
     client_aux.println("<p><strong>Espectro</strong></p>");
     client_aux.println("<img src='https://sylvania.blob.core.windows.net/assets/Hybris/0020966/Photometry/EN/0020966%20GroLux%20LED%20Flowering%20Spectrum.jpg' alt='spectrum' style='width:30%;height:30%;'>");
     client_aux.println("<p style='color:Gray;'>");
     client_aux.println("(*) DLI corrigido pelo fotoperiodo, ");
     client_aux.println("(**) DLI considerando um dia completo");
     client_aux.println("<p><strong>Download dos arquivos de dados:</p></strong>");
     client_aux.println("<p>");
     client_aux.println("<a href='/DATA.TXT'>Dados sensores</a> | <a href='/LOG.TXT'>log - controles</a>");
     client_aux.println("</p>");
     }

//Funcao Auiliar 8 - Abre um arquivo de texto contio no SD e envia para o client web
void write_from_file(EthernetClient &client, char * filename){
  File webFile = SD.open(filename);
  if (webFile) {
    while(webFile.available()) {
      client.write(webFile.read());
      Serial.println("SD lido"); 
    }
    webFile.close();
  } 
  else {
    Serial.print("Erro SD CARD: ");
    Serial.println(filename);
  }
}

//Funcao auxiliar 9 - Se o cliente requisitou o download de um arquivo de texto, essa função é acionada
void txt_file_download(EthernetClient &client, char * filename){
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/download");
  client.println("Connection: close");
  //abre o arquivo contido no SD    
  write_from_file(client, filename);
}
