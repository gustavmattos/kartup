# KartUp 2.2.0 — atualização pelo iPad

## Primeira instalação: cabo USB

Instale o PlatformIO no computador conectado por USB ao KartUp. Na pasta do projeto:

```sh
pio run -e cyd -t upload --upload-port COM3
```

Troque COM3 pela porta do dispositivo no computador. Esse comando grava bootloader,
tabela de partições e aplicação. Não basta gravar somente a aplicação se as partições
instaladas forem diferentes. Não é necessário apagar o cartão SD.

Na VPS, o binário de aplicação é `.pio/build/cyd/firmware.bin`.
O pacote `artifacts/kartup-2.2.0-ota-usb.bin` reúne os componentes da primeira
instalação e deve ser gravado **no endereço 0x0**, em um ESP32 CYD com flash de 4 MB.
Com esptool instalado no computador conectado ao dispositivo:

```sh
python -m esptool --chip esp32 --port COM3 write_flash 0x0 kartup-2.2.0-ota-usb.bin
```

Use alimentação estável. O arquivo combinado é exclusivo para instalação por cabo;
**não envie o arquivo `-usb.bin` pela página OTA**.

## Próximas atualizações: iPad + Wi-Fi

1. Compile a nova versão com `pio run -e cyd` na VPS. Preserve o suporte OTA nas versões futuras.
2. Transfira `.pio/build/cyd/firmware.bin` para o app Arquivos do iPad (por exemplo,
   com SFTP no Termius, se disponível na sua instalação). Faça isso antes de conectar à rede do KartUp.
3. Encerre a sessão de cronometragem. Abra Settings → WLAN no KartUp.
4. Conecte o iPad à rede `KartUp-Telemetria`; mantenha a conexão mesmo sem internet.
5. No Safari, abra `http://192.168.4.1` e escolha **2. Atualizar firmware** no menu inicial,
   ou abra diretamente `http://192.168.4.1/update`.
6. Usuário: `admin`. Senha: os oito dígitos mostrados na tela do KartUp.
   Uma nova senha é gerada a cada ativação do Wi-Fi.
7. Escolha `firmware.bin`, confirme e aguarde envio, validação e reinicialização.
8. Confira a versão na tela inicial/Settings. O Wi-Fi desliga após reiniciar;
   ative-o novamente para voltar à interface web.

A atualização não depende do cartão SD nem de internet no KartUp. Se o SD estiver
indisponível, a página de firmware continua funcionando.

## Implementação e limites

- Framework Arduino-ESP32 2.0.17, plataforma PlatformIO espressif32 6.12.0 fixada.
- Usa o servidor WebServer existente e a biblioteca nativa Update; sem Bluetooth.
- Tabela `min_spiffs.csv`: dois slots OTA; cada aplicação precisa caber no slot.
- Somente modo Wi-Fi explícito, fora de cronometragem. Autenticação na página e no
  recebimento, com token por sessão para impedir envio originado de outra página.
- Grava em blocos na partição inativa, sem carregar o firmware inteiro na RAM.
- Reinicia apenas após conclusão e validação aceitas por Update. Cancelamentos e
  falhas de gravação abortam a operação.
- A rede permanece aberta como antes; autenticação HTTP não criptografa o tráfego.
  Use em ambiente de confiança. Isso não implementa assinatura criptográfica do firmware.
- A validação do formato ESP32 não prova que o arquivo pertence ao KartUp:
  selecione apenas o binário correto para este hardware.
- Rollback automático após uma aplicação defeituosa **não foi habilitado**.
  Uma versão que não inicialize ou que remova o OTA pode exigir recuperação por cabo.
- Compilação e tamanho podem ser verificados na VPS; envio real pelo Safari,
  tela, GPS e recuperação precisam de teste no aparelho.

## Verificação no aparelho

Após a instalação USB, confirme tela, touch e GPS. Com uma sessão encerrada, teste
login incorreto (deve negar acesso), login correto e envio do mesmo `firmware.bin`:
deve reiniciar e continuar na versão 2.2.0-ota. Confira downloads CSV e uma nova
sessão de cronometragem. Um arquivo de texto renomeado para `.bin` deve ser rejeitado
sem reinicialização. Para atualizar em pista, conclua primeiro esse teste local.

## Menu do modo AP (2.2.1)

A página inicial `/` oferece duas opções: **1. Baixar logs** abre `/logs`,
e **2. Atualizar firmware** abre `/update`. A lista de logs tem um botão para
voltar ao menu. O menu e a atualização continuam disponíveis sem cartão SD.

Se `/update` não existir no aparelho, confirme se o firmware instalado já tem
suporte OTA. Alterar o código na VPS não atualiza o CYD: um aparelho com firmware
anterior ao OTA precisa receber a primeira instalação por USB.

## Login e versão no display (2.2.2)

Ao abrir `http://192.168.4.1`, o navegador solicita usuário `admin` e a senha
mostrada no CYD ao ativar WLAN. Menu, logs, downloads e logo exigem autenticação;
a atualização mantém a autenticação e o token por sessão. A senha muda ao reativar
WLAN, e o navegador pode reutilizar as credenciais durante o acesso.

A tela inicial mostra `Firmware 2.2.2-ota` centralizado abaixo do botão START,
com fonte 2. A barra decorativa e o texto no canto da engrenagem foram removidos.

No aparelho, conferir login incorreto/correto no menu, acesso direto a `/logs`
e `/download`, download CSV, atualização OTA e posição da versão após reiniciar.
