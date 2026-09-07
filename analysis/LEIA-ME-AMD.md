# OptiScaler AMD Pre-SR Multipass — build local v1.1

Esta build usa o runtime AMD v0.2.14 da `version.dll` fornecida e o fork OptiScaler-DLSSNR-PreSR-Multipass. O fluxo implementado é:

`cor na resolução interna → NR AMD/HIP (1–3 passagens) → FSR → saída do jogo`

A versão 1.1 acrescenta conversão de profundidade com shader para R32_FLOAT, incluindo texturas typeless de depth-stencil D16, D24S8, D32 e D32S8 com leitura por shader permitida. A versão 1 rejeitava qualquer recurso com ALLOW_DEPTH_STENCIL. O log agora registra dimensões, formato e flags da cor, movimento e profundidade para distinguir as causas de incompatibilidade. A captura do Cyberpunk mostrou o erro genérico da versão 1; a causa exata e a execução em jogo ainda precisam ser confirmadas no novo log.

## Instalação

1. Feche o jogo. Extraia o pacote inteiro em uma pasta separada.
2. Execute no PowerShell:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\INSTALAR_AMD.ps1 -GameDir "C:\caminho\da\pasta\do\executavel"
   ```

3. Ative FSR no jogo e escolha Quality/Performance. Abra o menu do OptiScaler com **Insert**. Em **DLSS Neural Rendering**, ajuste **AMD neural passes** entre 1 e 3.

O instalador salva os arquivos substituídos em `backup-amd-presr-DATA-HORA`. Carrega o novo OptiScaler como `dxgi.dll` e desativa a `version.dll` original analisada, colocando-a no backup. Caso exista outra `version.dll`, ele interrompe a instalação para evitar substituir um proxy desconhecido. Não execute o `setup_windows.bat` das builds NVIDIA neste pacote.

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

O usuário confirmou funcionamento da versão 1 no Onimusha. A versão 1.1 passou nos testes sintéticos com profundidade D16, D24S8, D32 e D32S8, recorte, mudança de resolução e reset, além da regressão com R32_FLOAT. **O Cyberpunk ainda precisa ser testado com esta atualização.** O teste sintético confirma a inferência e a sincronização do adaptador, mas não cobre as particularidades de integração de cada jogo. Seus tempos não são um benchmark de desempenho em jogo.

## Diagnóstico

Na pasta do jogo:

- `amd_presr.log`: inicialização das passagens e dimensões realmente processadas. Procure `Completed AMD pre-SR`.
- `dlssnr_on_amd.log`: processamento HIP e histórico por passagem.
- `OptiScaler.log`: interceptação do jogo e upscaler selecionado.

Compare a dimensão em `amd_presr.log` ao trocar Quality/Performance. Ela deve acompanhar a resolução interna, enquanto a resolução de saída permanece a mesma. Esse é o teste direto do pre-SR.

## Limites conhecidos

Somente uma queue de renderização e uma avaliação pendente são aceitas. Uma chamada enquanto a anterior ainda está pendente é ignorada, sem substituir a entrada do upscaler. O callback após ExecuteCommandLists libera cada passagem em ordem, porque o runtime usa o stream HIP padrão. Essa primeira implementação prioriza correção; o custo da espera CPU também precisa ser medido no jogo.

Motion vectors em resolução de exibição com dimensões diferentes da entrada, origem de cor diferente de zero, MSAA, profundidade sem leitura por shader e formatos de profundidade fora da lista acima não são suportados por esta build. Um erro de backend é registrado e requer reiniciar o jogo. Frame generation, DX11 e Vulkan não receberam validação em jogo.

O arquivo `version.dll` original na pasta de trabalho permanece intacto. As três DLLs privadas têm somente dois patches: desativar a instalação autônoma de hooks e impedir que a rotina de notificação reenvie command lists. O OptiScaler fornece inicialização, textura pre-SR e notificação de submissão. Este pacote contém os pesos locais do usuário e não foi publicado ou enviado a terceiros.
