# Montar o painel num notebook Windows

O painel foi feito pra Linux. No Windows a gente roda ele dentro do WSL2, que e um Ubuntu de verdade rodando junto do Windows. Depois de montado, a equipe usa so pelo navegador, nao precisa mexer no terminal.

Esse guia e pra quem vai montar o servidor uma vez. Ja tem que estar logado numa conta do Windows com permissao de administrador.

## O que esse notebook aguenta

O caso normal (leilao do YouTube com legenda automatica) roda tranquilo, porque a legenda vem pronta do YouTube e a extracao roda nos servidores da Anthropic pelo Claude. O local so orquestra.

O ponto fraco e a reserva: se o video nao tiver legenda, o notebook transcreve o audio na CPU, e num i5 com 8GB isso fica lento pra video longo. Entao o ideal e sempre usar links com legenda automatica. A transcricao local existe so pra nao travar de vez, nao pra ser o dia a dia.

## 1. Instalar o WSL2 com Ubuntu

Abra o PowerShell como administrador e rode:

    wsl --install -d Ubuntu

Reinicie o notebook quando ele pedir. Ao abrir o Ubuntu pela primeira vez, crie um usuario e uma senha do Linux (nao e a mesma do Windows).

Com 8GB de RAM vale limitar o WSL pra ele nao brigar com o Windows. Crie o arquivo `C:\Users\SEU_USUARIO\.wslconfig` com este conteudo:

    [wsl2]
    memory=6GB
    swap=4GB

Depois rode `wsl --shutdown` no PowerShell e abra o Ubuntu de novo. Isso da folga pra compilar.

## 2. Pegar o projeto

Dentro do Ubuntu (o terminal do WSL):

    sudo apt update && sudo apt install -y git
    git clone git@github.com:jpavrdev/audio-transcription.git
    cd audio-transcription

Se ainda nao tiver chave SSH no GitHub desse notebook, da pra clonar por https:

    git clone https://github.com/jpavrdev/audio-transcription.git

## 3. Instalar e logar o Claude Code

A extracao dos lotes usa o Claude Code no plano Max. Instale o Claude Code dentro do Ubuntu do WSL (a mesma forma que foi instalado na maquina de desenvolvimento) e rode uma vez pra logar:

    claude

Faca o login na conta Max. Sem isso a extracao nao roda.

## 4. Instalar o painel

Ainda dentro da pasta do projeto:

    ./scripts/instalar.sh

Ele instala as dependencias, compila o servidor e o transcritor, prepara o banco Postgres, aplica as migrations e pede a senha do admin. A primeira compilacao do Drogon demora, e normal.

## 5. Subir o painel

    ./scripts/iniciar.sh

Vai aparecer `servidor em http://localhost:8080`. Abra esse endereco no navegador do proprio notebook e entre com o usuario `admin`.

## Acessar de outros aparelhos na mesma rede

Se a equipe vai abrir o painel de outros computadores ou celulares na mesma rede, o Windows precisa repassar a porta pro WSL. No PowerShell como administrador:

    netsh interface portproxy add v4tov4 listenport=8080 listenaddress=0.0.0.0 connectport=8080 connectaddress=$(wsl hostname -I)
    netsh advfirewall firewall add rule name="painel leiloes" dir=in action=allow protocol=TCP localport=8080

Depois descubra o IP do notebook na rede (`ipconfig`, procure o IPv4) e os outros aparelhos acessam em `http://IP_DO_NOTEBOOK:8080`.

No Windows 11 da pra evitar isso ligando a rede espelhada: no `.wslconfig`, na secao `[wsl2]`, adicione `networkingMode=mirrored`, rode `wsl --shutdown` e suba de novo.

## Deixar subindo sozinho

O jeito simples e uma tarefa agendada do Windows que sobe o painel quando o notebook liga. No Agendador de Tarefas, crie uma tarefa que roda ao iniciar o Windows com:

    Programa:    wsl
    Argumentos:  -d Ubuntu -u SEU_USUARIO_LINUX -- /home/SEU_USUARIO_LINUX/audio-transcription/scripts/iniciar.sh

Ajuste os nomes de usuario e o caminho.

## Atualizar depois

Quando sair mudanca nova no projeto:

    cd audio-transcription
    git pull
    ./scripts/instalar.sh

O instalar.sh pode rodar de novo sem problema, ele reaproveita o que ja existe.
