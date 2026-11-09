Mars Colony: Project Red PS2 — homebrew para PlayStation 2.

**Arquivos**
- `MARSCOL.ELF` — executável sem compressão (o mais compatível). Rode pelo uLaunchELF/wLaunchELF (pendrive USB), PS2Link ou "Run ELF" no PCSX2.
- `MARSCOL_PACKED.ELF` — mesmo jogo comprimido (~1 MB), para carregadores com pouco espaço.
- `MarsColony_ProjectRed_PS2.iso` — imagem de disco para o PCSX2 ou OPL (NTSC, 640x448).

**Novidades da v1.5**
- Dois controles: o controle 1 comanda só o jogador 1 (Vermelho) e o controle 2 comanda os jogadores 2 a 4. Sem o controle 2 conectado, o controle 1 comanda todos, como antes.

**Novidades da v1.4**
- Áudio do PS2 reescrito: o jogo alimenta o servidor de som (audsrv) diretamente, inclusive enquanto a IA pensa. Corrige silêncio e estalos no AetherSX2.
- O `MARSCOL.ELF` principal agora vem sem compressão: ele carrega no endereço padrão e é aceito por mais emuladores. A versão comprimida carregava perto do fim da RAM.

**Novidades da v1.3**
- Corrige cores e transparência no PS2 e em emuladores: o chip gráfico (GS) usa 128 como 100% e o jogo enviava 255, deixando tudo com o dobro de brilho e opacidade.
- Corrige chiado e estalos no áudio: o som agora sai nos 48 kHz nativos do PS2, sem passar pelo conversor de taxa do IOP.

**Novidades da v1.2**
- Escolha de idioma ao iniciar: Português (principal), English e Español — tradução completa.
- Música e efeitos sonoros com tema espacial (3 trilhas originais, 13 efeitos).
- Arte enviada integrada: key art, ilustrações das 35 cartas, mascotes, prédios, fonte cromada e botões.
- 2 a 4 jogadores (humanos em hot-seat ou CPU fácil/normal/difícil), variante draft, tutorial de 9 páginas.

Observação: o jogo foi compilado com PS2SDK e testado na versão de PC (mesmo código). Ainda não foi testado em console real nem no PCSX2.
