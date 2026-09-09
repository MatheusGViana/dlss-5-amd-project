# RTGI Diffuse — avaliação local

Escopo: somente iluminação difusa. Avaliado o arquivo em rtgi/iMMERSE/MartysMods_RTGI_DIFFUSE.fx, versão declarada D-1.1, e os componentes compartilhados necessários ao seu funcionamento.

## Resultado

A integração direta em D3D12 é tecnicamente viável, mas exige um subsistema de renderização adicional. O arquivo não é HLSL independente: usa declarações de recursos, passes, interfaces e funções fornecidas pelo ambiente de efeitos. Não pode ser passado diretamente ao D3DCompile usado pelo filtro atual.

## Dependências identificadas

- Os 12 includes diretos estão na pasta MartysMods.
- As três texturas explícitas de ruído temporal e horizonte estão em Textures/iMMERSE.
- Os dados compartilhados incluem normais de superfície e geometria, movimento em UV e albedo. Sua preparação depende de partes do Launchpad, presente no pacote; isso é infraestrutura, não inclusão de outro efeito visual.
- Projeção da câmera, FOV, profundidade linearizada, escala de resolução, tempo e número do quadro precisam ser alimentados pelo host.
- Os addons encontrados são destinados a outros efeitos; não aparecem como dependências diretas do Diffuse examinado.

## Pipeline

São 17 passes declarados normalmente, com mais dois passes condicionais para reconstrução temporal de resolução. Incluem redução de profundidade, inicialização e quatro propagações de radiância, traçado, momentos espaciais, reprojeção temporal, atualização de histórico, seis etapas de filtragem e composição. O número de passes não permite estimar FPS sem medir a implementação.

O host precisa criar texturas 2D, uma textura 3D de radiância, mipmaps, descritores, pipelines de compute e pixel, render targets múltiplos e recursos persistentes de histórico. As barreiras e a duração dos recursos devem acompanhar a submissão efetiva do jogo.

## Adaptação proposta

1. Implementar e validar profundidade linear, normais e conversão dos vetores do jogo para deslocamento UV. Verificar FOV, profundidade invertida, jitter e resoluções diferentes.
2. Portar as etapas de radiância e traçado para HLSL/recursos D3D12 explícitos.
3. Portar a reprojeção e o denoiser com históricos próprios, reiniciados em cortes de câmera, resize e troca de upscaler.
4. Integrar a composição em uma etapa que preserve a convenção de cor e exposição. A posição final relativa ao neural precisa de comparação visual; não presumir que a composição original funcione diretamente em cor linear pré-exposta.
5. Expor qualidade, oclusão ambiente, iluminação indireta, espessura, qualidade/suavidade do denoiser, nível ambiente, distância de efeito, diagnóstico e restauração de padrões.
6. Medir GPU/VRAM e testar movimento de câmera e FG. Executar o efeito somente em quadros renderizados, antes da geração de quadros, com sincronização própria.

## Estado

Avaliação de viabilidade e dependências concluída. Nenhum shader RTGI foi incorporado à DLL ou ao pacote de distribuição nesta etapa. A versão instalada permanece inalterada.
