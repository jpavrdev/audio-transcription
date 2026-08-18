#pragma once
// registra as rotas de usuarios, so admin (RequerAdmin):
//   GET  /usuarios   lista os usuarios
//   POST /usuarios   cria um usuario {usuario, senha, papel}
void registrar_usuarios();
