# OptiScaler AMD Pre-SR Multipass — build local v1.6

Esta build usa o runtime AMD v0.2.14 da `version.dll` fornecida e o fork OptiScaler-DLSSNR-PreSR-Multipass. O fluxo implementado é:

`cor na resolução interna → NR AMD/HIP (1–3 passagens) → FSR → saída do jogo`

## Novidades da v1.6

- Protege o upload de texturas do menu contra falhas de criação, mapeamento e perda do dispositivo. O RVA 0x1469a4 da v1.5 corresponde ao uso do buffer sem verificar a falha de criação. O log do Requiem mostrou a perda da GPU ANTES dessa violação de acesso; a proteção evita esse acesso inválido secundário, não recupera uma GPU já perdida nem comprova a correção da causa inicial do hang.
- O backend AMD deixa de gravar trabalho após detectar perda do dispositivo.
- Invalida o histórico ao desligar/reativar NR, desativar pre-SR, mudar tom/estrutura/pele e após intervalo sem processamento superior a 250 ms. O descarte ocorre no próximo ponto seguro, depois da conclusão do trabalho anterior. Isso reduz a possibilidade de reutilizar histórico incompatível; a melhora visual após reiniciar o jogo ainda precisa de comparação em jogo.
- O instalador preserva o nome do proxy OptiScaler existente (por exemplo WINMM.dll) e interrompe se identificar duas cópias de proxy OptiScaler. Para instalação nova continua usando dxgi.dll.

Testes: upload/destruição normal de textura e atualização após perda provocada de um dispositivo WARP isolado, sem acesso inválido; teste AMD verificando a invalidação do histórico ao alternar parâmetros, resolução e passagens. Build Release x64 concluída. A origem do DXGI_ERROR_DEVICE_HUNG com FG continua em investigação; não há garantia de que v1.6 elimine o crash inicial do jogo.

## Novidades da v1.5

- Antes de inserir trabalho AMD, observa uma submissão da lista SR pelo hook. Durante o aquecimento, o jogo recebe sua imagem normal. Se o painel continuar em `waiting to observe SR list submission`, o efeito permanece em bypass: envie os novos logs para investigar o caminho de submissão do jogo/proxy.
- O primeiro worker é avisado imediatamente antes de ExecuteCommandLists, sem aguardar nessa etapa. Ele espera a captura na GPU. As outras passagens mantêm a ordem após a submissão. Isso evita depender do retorno de ExecuteCommandLists para iniciar o primeiro worker.
- O shader de espera tem limite adicional de 262.144 iterações. Mesmo sem notificação o shader pode terminar pelo limite, preservando a imagem atual. Esse limite não é uma garantia de duração fixa em milissegundos ou de ausência de TDR em todos os cenários.
- Mantém a recuperação de timeout, histórico e mudanças de resolução da v1.4. As três DLLs AMD mudaram novamente: atualize-as junto com o OptiScaler.

O teste de notificação atrasada terminou a lista GPU antes de avisar o worker (62 ms na execução registrada), verificou saída idêntica à entrada e recuperação posterior. O teste com três passagens, profundidade D32S8, troca de filas e resolução passou. Isso não valida o hook de todos os jogos: Requiem e Silent Hill f ainda precisam de teste em jogo. O log recebido do Silent Hill f é de v1.3 e para depois de gravar o primeiro quadro; não demonstra a conclusão do worker nem determina a causa exata do crash. O log do Requiem v1.4 registra DXGI_ERROR_DEVICE_HUNG.

## Novidades da v1.4

- Se uma passagem exceder o tempo de espera, seu shader preserva a entrada atual. Não reutiliza o residual do quadro anterior, que podia produzir rastros sob movimento e FG.
- A marca de cancelamento obsoleta é limpa antes de submeter o novo trabalho, depois da conclusão do anterior. Corrige a repetição de cancelamentos após os IDs de jobs reiniciarem numa mudança de resolução. O watchdog continua ativo para o novo job.
- Ao detectar timeout, o NR pausa por um segundo para aliviar a carga e reinicia o histórico ao tentar novamente. O jogo continua com sua imagem normal durante essa pausa; isso pode causar mudança temporária na aparência. A pausa não desativa o upscaler nem altera a configuração de FG.
- A presença dos arquivos e a identificação do adaptador são armazenadas em cache para reduzir trabalho de CPU por quadro. Não houve benchmark comparativo de FPS.
- O painel informa `timeout events`. `completed frames` conta conclusões de processamento, incluindo casos de fallback; não comprova que o efeito foi aplicado em todos os quadros.

**Atualização:** substitua `dxgi.dll` (a nova `OptiScaler.dll` renomeada) E as três `dlssnr_amd_pass*.dll` juntos. O hash dos runtimes mudou e a build rejeita combinações de versões. O instalador do pacote faz essas cópias com backup.

Testes locais: timeout induzido após dois quadros válidos, saída idêntica à entrada no quadro cancelado, pausa e recuperação, mudança de resolução e combinação com três passagens/depth-stencil/troca de filas. Os testes passaram na RX 9060 XT. O log recebido da RX 9070 XT confirma inicialização e milhares de quadros processados antes de `DXGI_ERROR_DEVICE_HUNG`, precedido de esperas de captura de 2–3 segundos. Esses logs não bastam para provar a causa exata do hang. A v1.4 ainda precisa de validação em jogos com FG 3x+ e na máquina do tester; não há promessa de eliminar todo ghosting temporal ou todos os crashes.

A versão 1.3 corrige `AMD pass rejected frame: 1` ao trocar a resolução imediatamente após o primeiro quadro. A recriação dos buffers internos reinicia o contador de jobs; a versão 1.2 confundia um contador repetido com rejeição, deixando trabalho gravado sem notificação. Agora a confirmação usa o command list pendente do runtime. Mesmo em caso de falha, passagens já gravadas são notificadas após submissão para não abandonar a espera da GPU.

O defeito foi reproduzido no teste sintético antes da correção, com encerramento anormal. Após a correção, oito quadros alternando resolução desde o segundo quadro passaram com saída finita e encerramento normal. Também passou a combinação de três passagens, profundidade D32S8 e troca de filas. Os logs do Resident Evil mostram exatamente a transição após o primeiro quadro (3440x1440 para 1136x640 ou 1280x720). A confirmação da v1.3 em jogo e as causas dos relatos de crash em Unreal dependem de novos testes/logs.

A versão 1.2 identifica a submissão pelo command list processado, em vez de exigir a fila de apresentação inicial. Isso corrige a rejeição quando o FG troca a fila do swapchain e permite acompanhar mudanças na fila que executa o render. O hook usa a implementação da fila do dispositivo para evitar depender de proxies de apresentação. O painel informa quantos quadros foram processados e a idade da última conclusão. A configuração de FG permanece sob controle do jogo/usuário.

Teste: ative o FG pelo jogo e confira se `completed frames` continua aumentando. Depois teste o multiframe pelo OptiScaler separadamente. Os testes sintéticos alternaram filas reais D3D12 com uma fila de apresentação separada, três passagens, mudança de resolução e profundidade D32S8. **FG nativo e multiframe ainda precisam de confirmação em jogo com a versão 1.2.**

A versão 1.1 acrescenta conversão de profundidade com shader para R32_FLOAT, incluindo texturas typeless de depth-stencil D16, D24S8, D32 e D32S8 com leitura por shader permitida. A versão 1 rejeitava qualquer recurso com ALLOW_DEPTH_STENCIL. O log agora registra dimensões, formato e flags da cor, movimento e profundidade para distinguir as causas de incompatibilidade. A captura do Cyberpunk mostrou o erro genérico da versão 1; a causa exata e a execução em jogo ainda precisam ser confirmadas no novo log.

## Instalação

1. Feche o jogo. Extraia o pacote inteiro em uma pasta separada.
2. Execute no PowerShell:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\INSTALAR_AMD.ps1 -GameDir "C:\caminho\da\pasta\do\executavel"
   ```

3. Ative FSR no jogo e escolha Quality/Performance. Abra o menu do OptiScaler com **Insert**. Em **DLSS Neural Rendering**, ajuste **AMD neural passes** entre 1 e 3.

O instalador salva os arquivos substituídos em `backup-amd-presr-DATA-HORA`. Preserva o nome de um proxy OptiScaler já instalado ou usa `dxgi.dll` numa instalação nova. Desativa a `version.dll` AMD original analisada, colocando-a no backup; uma `version.dll` identificada como OptiScaler é preservada como nome do proxy. Outra `version.dll` desconhecida interrompe a instalação. Não execute o `setup_windows.bat` das builds NVIDIA neste pacote.

Para restaurar, feche o jogo e copie os arquivos do backup de volta, incluindo `version.dll` quando presente. Se não existia `dxgi.dll` antes da instalação, remova a nova `dxgi.dll`. `manifest.json` registra quais arquivos já existiam.

## Configuração desta build

- NR ativado, pre-SR ativado, uma passagem por padrão.
- Upscaler DX12: `ffx` (FSR). Frame generation desligado inicialmente para isolar a validação.
- Cor convertida para FP16 no tamanho ativo. Texturas de motion/depth com padding são recortadas para a mesma área de origem zero.
- Cada passagem usa uma instância separada do runtime, com buffers e histórico próprios. As avaliações são sequenciais. O ajuste de tom só é aplicado na primeira passagem.
- As passagens AMD compõem sequencialmente sobre a cor. Isso difere da composição final única do backend NGX do fork NVIDIA. Os perfis/presets NVIDIA e WorkingScale não são controles do backend AMD desta build.
- O processamento inline aguarda o resultado do mesmo frame. Mais passagens aumentam uso de VRAM e custo. O pacote não promete um FPS específico.

## Verificação realizada

O código do OptiScaler foi compilado em Release x64. O mesmo adaptador AMD foi executado em um teste D3D12 na RX 9060 XT, com a DLL fornecida e seus pesos:

- Uma, duas e três passagens produziram alterações numéricas reais na imagem.
- Testes de frames consecutivos, textura com padding, mudança de resolução ativa e reset de histórico.
- Logs do runtime confirmam histórico temporal ativo após aquecimento.
- Saídas testadas sem NaN/Inf; inspeção das mensagens de erro D3D12 quando a camada de debug está disponível.
- Encerramento dos workers fora do loader lock validado pelo teste.

O usuário confirmou funcionamento da versão 1 no Onimusha e da versão 1.1 no Cyberpunk sem FG. A versão 1.1 passou nos testes sintéticos com profundidade D16, D24S8, D32 e D32S8, recorte, mudança de resolução e reset, além da regressão com R32_FLOAT. O teste sintético confirma a inferência e a sincronização do adaptador, mas não cobre as particularidades de integração de cada jogo. Seus tempos não são um benchmark de desempenho em jogo.

## Diagnóstico

Na pasta do jogo:

- `amd_presr.log`: inicialização das passagens e dimensões realmente processadas. Procure `Completed AMD pre-SR`.
- `dlssnr_on_amd.log`: processamento HIP e histórico por passagem.
- `OptiScaler.log`: interceptação do jogo e upscaler selecionado.

Compare a dimensão em `amd_presr.log` ao trocar Quality/Performance. Ela deve acompanhar a resolução interna, enquanto a resolução de saída permanece a mesma. Esse é o teste direto do pre-SR.

Em outra máquina, execute `powershell -ExecutionPolicy Bypass -File .\DIAGNOSTICO_AMD.ps1 -GameDir "C:\pasta\do\jogo" > diagnostico-amd.txt` a partir do pacote e compartilhe o relatório para investigar falhas. O script somente lê arquivos, versões e informações das GPUs; o relatório inclui caminhos locais. A DLL original importa `amdhip64_7.dll`: HIP 6 sozinho não atende essa dependência. A build registra o caminho do HIP carregado e códigos de erro de enumeração/carregamento. Não copie DLLs de sistema avulsas entre computadores.

A RX 9060 XT usa gfx1200, enquanto a RX 9070/9070 XT usa gfx1201, conforme a documentação AMD: https://rocm.docs.amd.com/projects/install-on-windows/en/latest/reference/system-requirements.html . O runtime fornecido contém código para ambos os alvos; testes locais foram feitos somente na RX 9060 XT. Falhas relatadas em outras máquinas ainda exigem logs para diagnóstico.

## Limites conhecidos

Somente uma avaliação pendente é aceita. A fila de renderização pode mudar entre avaliações, depois da conclusão da anterior. Uma chamada enquanto a anterior ainda está pendente é ignorada, sem substituir a entrada do upscaler. O callback após ExecuteCommandLists libera cada passagem em ordem, porque o runtime usa o stream HIP padrão. O custo da espera CPU também precisa ser medido no jogo.

Motion vectors em resolução de exibição com dimensões diferentes da entrada, origem de cor diferente de zero, MSAA, profundidade sem leitura por shader e formatos de profundidade fora da lista acima não são suportados por esta build. Um erro de backend é registrado e requer reiniciar o jogo. Frame generation, DX11 e Vulkan não receberam validação em jogo.

O arquivo `version.dll` original na pasta de trabalho permanece intacto. As três DLLs privadas têm dois patches de instruções (desativar a instalação autônoma de hooks e impedir o reenvio de command lists), mais a alteração do shader de fallback, sua mensagem de log e o limite adicional do shader de espera. O OptiScaler fornece inicialização, textura pre-SR e notificação de submissão. Este pacote contém os pesos locais do usuário e não foi publicado ou enviado a terceiros.
