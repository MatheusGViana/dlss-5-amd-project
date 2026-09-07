# Engenharia reversa inicial — DLL AMD v0.2.14

## Resultado e limites

A análise estática identificou a interface interna de captura/composição e o papel do hook de submissão D3D12. Ainda não foi produzida uma DLL pre-SR funcional nem feita validação em jogo. A DLL original e os arquivos instalados do jogo foram preservados.

Arquivo analisado: `../version.dll`, 7.156.224 bytes, PE x64.

SHA-256: `106223723fd9266c44d38dc2fb77933948ab37803f46bfcea2bae3a0a474ac84`.

Esse hash coincide com `../../engenharia-reversa/dlssnr_amd_v0214.dll`. Portanto, os dumps Ghidra existentes para essa imagem são úteis; os principais pontos abaixo também foram conferidos independentemente com Capstone na DLL desta pasta. Dumps antigos permanecem material auxiliar, não prova de funcionamento das modificações anteriores.

## Arquitetura observada

- 17 exports, todos da API de versão do Windows. Não exporta NGX nem uma API pública para executar a rede.
- Importa `amdhip64_7.dll`; seção `.hip_fat` com 6.649.344 bytes. Identificadores de alvos incluem gfx1200 e gfx1201, além de outros alvos AMD.
- Strings indicam pesos em `dlssnr_on_amd_weights.bin`, ou sua construção a partir de `nvngx_dlssnr.dll`. A presença de strings não valida numericamente a rede ou a equivalência dos resultados.
- Intercepta FSR, D3D12 e DXGI. O log fornecido registra inicialização HIP numa RX 9060 XT e trabalhos de rede concluídos.

O repositório público do porte contém documentação e releases, sem o fonte do runtime na árvore consultada: https://github.com/danielblnc/DLSS-NR-on-AMD .

## Funções localizadas

RVA significa endereço relativo à base do módulo; não é offset de arquivo. Endereços abaixo pertencem exclusivamente ao hash acima.

| RVA | Papel observado | Evidência |
|---|---|---|
| `0x97E0` | Wrapper FSR3 ContextDispatchUpscale | Chama original em `0x97E9`, depois `0xA0B0` em `0x98A9` |
| `0x9A40` | Wrapper FSR3 UpscalerContextDispatch | Chama original em `0x9A49`, depois `0xA0B0` em `0x9B09` |
| `0x10110` | Wrapper comum ffxDispatch | Chama original em `0x10122`; busca descritor tipo `0x10001`; chama `0xA0B0` em `0x10205` |
| `0xA0B0` | Captura, staging e composição | Recursos do pacote; consulta GetDesc; converte estados FFX; prepara trabalho inline/async |
| `0x4640` | Hook ExecuteCommandLists | Encaminha original, procura command list pendente, publica slot ou sinaliza fence |
| `0xC7D0` | Rotina de captura/cópia | Recebe command list, recurso, estado FFX, staging e índice de slot |
| `0xD270` | Preparação de staging | Consulta recurso e footprints; cria/importa buffers de interop |
| `0xC9E0` | Leitura/normalização de exposição | Lê valor de exposição via HIP ou Map; não é a inferência |

Os três wrappers executam NR após a chamada FSR original. Isso confirma a ordem de gravação das operações; retornar de `ffxDispatch` NÃO significa que a GPU terminou ou que a command list já foi submetida.

## Contrato reconstruído

`recovered_abi.h` documenta um pacote de 0x50 bytes, com ponteiros para command list, cor, motion vectors, profundidade e exposição; estados FFX para cada recurso; dois floats de escala dos vetores. Os nomes são reconstruídos, não símbolos originais.

A rotina consulta a descrição completa da textura de cor (`GetDesc`, chamada em `0xA265`) e compara largura/altura/formato com o staging (`0xA530` em diante). Os wrappers não transferem renderSize para esse pacote. Para pre-SR com recursos alocados no tamanho máximo, só trocar o ponteiro de output pelo de color pode manter o processamento em resolução excessiva. É necessário tratar a área ativa.

Os estados recebidos NÃO são D3D12_RESOURCE_STATES diretamente. Há conversão de bits dentro do runtime. Motion/depth não podem receber um estado presumido sem conferir o contrato do chamador.

## Sincronização: dependência confirmada

No hook `0x4640`:

1. `0x4653`: chama a função original de ExecuteCommandLists.
2. `0x46D8`: lê o ponteiro global da command list pendente em RVA `0x76D68`.
3. `0x4700`: procura esse ponteiro no array de listas realmente submetidas.
4. `0x4715`: toma posse do registro pendente com compare/exchange.
5. `0x4725`: retira o slot pendente em `0x76D70`.
6. Conforme o modo, publica trabalho para o consumidor ou chama Signal na queue (`0x477E`).

A rotina `0xA0B0` escreve a command list pendente nos caminhos observados. Consequentemente, chamar só essa rotina não equivale a chamar um motor independente. A inicialização, o estado global e o mecanismo de submissão também participam do protocolo.

Isso explica por que remover o hook de queue exige implementar um substituto equivalente. Ainda NÃO demonstra sozinho a causa de cada travamento anterior; para isso faltam traces de execução e do consumidor HIP.

Uma fila privada também não resolve automaticamente: ela precisa aguardar a produção real da cor/depth/motion pelo jogo, e a fila do jogo precisa consumir o resultado na ordem correta. Aguardar no CPU algo que depende de uma lista ainda não submetida pode bloquear o progresso. Esse risco precisa ser avaliado por caminho, não atribuído genericamente a toda chamada pre-SR.

## Material anterior encontrado

Em `../../OptiScaler-DLSSNR-PreSR-Multipass/OptiScaler/dlssnr/DlssNr_AmdBackend.cpp`, o caminho v0.2.14 contém um `return true` antes da criação da textura compacta e da chamada pre-SR. O código abaixo desse retorno é inalcançável naquele ramo. O comentário informa que o caminho foi desabilitado para restaurar o hook FFX original.

Esse fonte fica fora do snapshot limpo desta pasta e não foi alterado. Sua existência não comprova quais binários do jogo foram compilados a partir dele. A afirmação no comentário de que estar depois do dispatch equivale a já estar submetido precisa ser corrigida conceitualmente: dispatch grava comandos; ExecuteCommandLists os submete.

## Integração proposta, ainda não implementada

Fluxo pretendido: entrada do jogo em resolução ativa → NR AMD/HIP → upscaler compatível com AMD, como FSR → saída final.

O fork NVIDIA fornece o ponto de inserção pre-SR, mas seu backend NGX não se torna um backend HIP ao renomear a DLL. Na RX, a seleção DLSS de um jogo pode ser traduzida pelo OptiScaler para outro upscaler; isso é separado da execução do NR.

Próximo experimento deve usar uma passagem, preservar o protocolo inline original e validar captura/composição numa textura compacta antes do FSR. Precisa de:

1. Mapa completo de inicialização, worker, flags e slots, com identificação do produtor e consumidor de cada sinal.
2. Supressão somente da avaliação post-SR duplicada, sem apagar a sinalização necessária de ExecuteCommandLists.
3. Cor em dimensões ativas; cópia/crop e tratamento de formatos/estados; correspondência de motion/depth e seus fatores de escala.
4. Reset e vida útil do histórico nas mudanças de resolução; buffers não reutilizados enquanto em voo.
5. Confirmação de que o FSR lê a cor transformada no mesmo frame e de que não há espera circular.
6. Teste em jogo: baseline desligado, NR post-SR original e NR pre-SR com resolução de entrada registrada. Medir separadamente tempo de inferência, espera e tempo total de frame; testar movimento e mudanças Quality/Performance.

Não habilitar multipass até validar a posse do histórico por passagem; o runtime AMD observado usa estado global e isso não equivale às features NGX independentes do fork.

## O que o log permite concluir

O log registra staging de cor 2560×1080 e trabalhos na faixa de 62–79 ms em diversos frames inline, além de amostras de inicialização mais lentas. Esses valores não são uma medição isolada do custo dos kernels nem uma previsão de FPS após a adaptação. O log também registra esperas, mudança para async e retorno para inline.

Motion vectors com média zero aparecem repetidamente; isso merece teste com câmera em movimento, mas pode corresponder a cena parada. Não é prova isolada de defeito.

## Artefatos reproduzíveis

- `inspect_pe.py`: exports, imports, seções, strings e hash; resultados em `version-pe.json` e `version-strings.txt`.
- `disassemble_hooks.py`: localiza referências às mensagens dos hooks e exporta suas funções.
- `disassemble_frame.py`: exporta a função `0xA0B0` com anotações de strings.
- `disassemble_sync.py`: exporta hook da queue, captura e exposição.
- `recovered_abi.h`: layout documentado, sem código para carregar/invocar a DLL.

Executar os scripts com cwd na pasta `funcional`; dependências locais em `.analysis-tools` (pefile e Capstone). A desassemblagem é estática; nenhum dos scripts carrega a DLL para execução.
