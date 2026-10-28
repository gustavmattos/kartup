# Firmware KartUp CYD

Leia CLAUDE.md para contexto técnico e restrições de hardware.

## Versionamento

Por decisão do usuário, a nova série inicia em **0.0.1 OTA** (substitui a numeração histórica 2.2.x).
Sempre incremente a versão ao preparar um novo firmware para entrega; nunca publique binários diferentes com a mesma versão.
Use patch para ajustes/correções, minor para funcionalidades e major para mudanças incompatíveis.
Mantenha FIRMWARE_VERSION em src/main.cpp, CHANGELOG.md, nome do binário e tag da Release consistentes.
Formato no produto: `0.0.1 OTA`; tag: `v0.0.1-ota`; arquivo: `firmware-0.0.1-ota.bin`.
Compile com `/root/kartup-build-env/bin/pio run -e cyd` nesta VPS. Registre limites de validação física.
