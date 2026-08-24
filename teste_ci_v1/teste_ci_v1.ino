// TESTE DA ACTION compilar-firmware.yml (24/08/2026)
// Sketch minimo, descartavel - so serve pra provar que a esteira de
// compilacao automatica funciona: instala o core ESP8266, instala a
// WiFiManager (mesma lib do Gateway de producao), compila, gera o .bin.
// Nao faz nada de util sozinho - nunca deve ser gravado num ESP de verdade.
// Depois de confirmado, pode apagar esta pasta do repositorio.

#include <ESP8266WiFi.h>
#include <WiFiManager.h>   // mesma lib usada no Gateway real - testa o lib install tambem

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println(F("[teste-ci] compilado com sucesso pela Action automatica"));
}

void loop() {
  delay(1000);
}
