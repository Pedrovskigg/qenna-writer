# Memórias da Tradução EN/US — Mira Writing

Documento de referência sobre a tradução do Mira Writing para inglês (2026-07-11).
Serve como guia pra futuras traduções (novos idiomas, ou manutenção da tradução EN
já existente conforme o app cresce).

## Arquitetura de i18n do projeto

- Qt Linguist padrão: `translations/mira_pt_BR.ts` (idioma-fonte) e
  `translations/mira_en.ts` (traduzido). Compilados em `.qm` via `lrelease` e
  embutidos no binário como recurso Qt (`qrc_mira-writing_translations`).
- `CMakeLists.txt` já tem `qt_add_translations(mira-writing TS_FILES ...)` configurado
  — o build recompila os `.qm` automaticamente quando os `.ts` mudam.
- Troca de idioma: `QSettings("app/language")`, lida em `src/main.cpp` ao carregar o
  `QTranslator` no boot. A UI de troca fica no rodapé do Main Menu
  (`src/MainMenuDialog.cpp`, combo `m_langCombo`) — ao trocar, o app salva a
  preferência e **reinicia sozinho** (`QProcess::startDetached` + `quit()`) pra
  carregar o novo `.qm`.
- Comandos principais:
  ```powershell
  # Escanear código-fonte por novas strings tr() e atualizar os .ts:
  & "C:\Qt\6.8.3\mingw_64\bin\lupdate.exe" src -ts translations\mira_pt_BR.ts translations\mira_en.ts

  # Compilar .ts → .qm (via CMake, roda lrelease):
  cmake --build build --target release_translations
  ```

## O que foi traduzido (escopo final)

- **~2.900 mensagens** no total, cobrindo toda a UI: editor, gavetas, temas,
  glossário, busca, Timeline, Pensário, Construtor (incluindo os ~1.200 templates
  de sistemas de magia/política/religião/economia/sociedade), Lousa, Mapa-múndi,
  Ficha de Personagem, Exportação, Configurações, etc.
- **283 quotes** do Main Menu (`src/_quotes_data.inc`) — não foram traduzidas
  literalmente, foram reescritas com liberdade pra soarem naturais em inglês,
  mantendo fatos, nomes de autores/livros e as piadinhas internas do app
  (P.H. Lobato, Tony Silva, o easter egg dos "404 quotes") intactas.
- **Nomes de países** no mapa-múndi: os dados GeoJSON já vinham com `NAME` (inglês)
  e `NAME_PT` (português) — só precisou fazer a seleção depender do idioma ativo
  do app em vez de sempre preferir PT.
- **Continentes e ~90 nomes de idiomas** (ex: painel de detalhes de país no mapa):
  eram tabelas hardcoded em português dentro de `src/MapView.cpp`, sem nenhuma
  função `tr()` — precisaram ser envolvidas manualmente.
- **121 nomes de Temas** (`src/Theme.cpp`): por decisão do usuário, **não foram
  internacionalizados** — foram só renomeados direto no código-fonte pra inglês
  (ex: Grafite→Graphite, Mármore→Marble, Meia-Noite→Midnight). Nomes que já eram
  em inglês ou nomes próprios (Nord, Dracula, Rosé Pine, Tokyo Night, Sakura,
  Bauhaus, Papyrus, VHS, Wabi-Sabi, Everforest, One Dark...) ficaram intactos.
  Ver mapeamento completo no histórico do commit / no próprio `Theme.cpp`.

## Metodologia (como foi feito)

1. `lupdate` pra extrair todas as strings `tr()` do código-fonte pros `.ts`.
2. Como o volume era grande (chegou a 1938 strings pendentes numa leva só), em vez
   de editar o XML mensagem por mensagem, o fluxo que funcionou bem foi:
   - Script Python extrai todas as `<source>` únicas com `type="unfinished"`,
     parseando por bloco `<message>...</message>` (nunca com regex direto no XML
     inteiro — ver bug abaixo).
   - Tradução feita em lote, em arquivos de texto simples `índice\tTEXTO`.
   - Script Python reaplica no `.ts`, faz o escaping XML (`&`, `<`, `>`, aspas,
     apóstrofo) e valida com `lrelease` (0 unfinished) + `xml.dom.minidom` (XML
     bem-formado) no final.
3. Terminologia consistente usada em toda a tradução: Gaveta→Drawer,
   Lousa→Board, Construtor→Builder, Manuscrito→Manuscript, Pensário→Pensarium,
   Menu de Referência→Reference Menu, Marcador→Marker, Elemento→Element.

## Bugs encontrados e corrigidos no processo

Esses são os pontos mais importantes pra não repetir em traduções futuras:

### 1. `lupdate.exe` não roda em automação não-interativa neste ambiente
`lupdate` (e às vezes `lrelease`) falha com "operação cancelada pelo usuário"
quando disparado via subprocesso sem uma sessão de usuário ativa na tela — parece
um prompt do Windows (SmartScreen/UAC-like) que precisa de alguém clicando.
**Sempre que precisar rodar `lupdate`, peça pro usuário rodar no terminal dele
mesmo**, com a janela em foco. Rodar duas instâncias ao mesmo tempo também gera
prompts duplicados e confunde — rodar uma де cada vez.

### 2. Classes sem `Q_OBJECT` quebram o contexto de tradução do `tr()`
`BookCard`/`BookRow` em `MainMenuDialog.cpp` são `QFrame` sem `Q_OBJECT`
(decisão intencional, pra evitar moc). Como não têm `tr()` próprio, o `tr()`
chamado dentro delas resolve — em tempo de compilação — pro `tr()` **herdado da
classe-base com `Q_OBJECT` mais próxima** (`QFrame`, no caso), usando o contexto
`"QFrame"` em runtime.

Só que o `lupdate` não sabe disso: ele escaneia o código léxicamente e atribui o
contexto pelo nome da classe onde o texto aparece (`"BookCard"`/`"BookRow"`),
**não** pelo contexto real de execução. Resultado: a tradução cai num contexto
que nunca é consultado em runtime — a string sempre aparece em português, mesmo
com a tradução certinha no `.ts`.

Pior: **editar o `.ts` manualmente pra adicionar um contexto `"QFrame"` fake não
resolve de forma duradoura** — na próxima vez que `lupdate` rodar, ele não vai
achar nenhuma referência real a esse contexto no código-fonte e vai marcar essas
mensagens como `type="vanished"` (obsoletas), que são ignoradas pelo `lrelease`.

**Fix correto**: dentro de classes sem `Q_OBJECT`, trocar chamadas `tr(...)`
soltas por `QCoreApplication::translate("NomeDaClasse", "...")` explícito — isso
faz o contexto em runtime bater com o que o `lupdate` já detecta corretamente
(o nome léxico da classe), sem precisar adicionar `Q_OBJECT`/moc nem hacks no `.ts`.

### 3. Strings traduzidas cacheadas em `QSettings` ficam presas no idioma antigo
`Quotes.cpp` monta um ciclo embaralhado de frases e salva em `QSettings`
(`quotes/cycle`) pra manter ordem estável entre sessões. Se o texto for
traduzido **antes** de cachear, o cache guarda o resultado já traduzido — e como
`QSettings` não é limpo ao trocar de idioma, o usuário fica preso vendo frases no
idioma antigo até o ciclo (283 itens) se esgotar naturalmente.

**Fix**: cachear sempre o texto **original, não traduzido**; traduzir só na hora
de exibir (`QCoreApplication::translate(contexto, textoOriginal.toUtf8().constData())`).
Regra geral: **nunca persista texto já traduzido em QSettings/arquivo** — persista
a chave/fonte original e traduza no consumo.

### 4. Dados estáticos (arrays de `const char*`, JSON, TSV) não usam `tr()` sozinhos
Três casos diferentes apareceram, cada um com solução própria:
- **`_quotes_data.inc`** (283 citações): array `const char* const[]` fora de
  qualquer classe. Solução: envolver cada literal com
  `QT_TRANSLATE_NOOP("Quotes", u8"...")` (marca pro `lupdate` extrair, sem efeito
  em runtime) e trocar os pontos de leitura de `QString::fromUtf8(...)` pra
  `QCoreApplication::translate("Quotes", ...)`.
- **`kContinents[]`** em `MapView.cpp` (rótulos desenhados no mapa): mesma
  solução — `QT_TRANSLATE_NOOP("QObject", "...")` na tabela +
  `QObject::tr(c.name)` na leitura.
- **Nomes de países/estados no GeoJSON** (`GeoData.cpp`): já vinham com campo
  bilíngue pronto (`NAME`/`NAME_PT`) — não precisou de `tr()`, só mudar a lógica
  de seleção pra depender de `QSettings("app/language")` no load.

### 5. Bug de escaping ao extrair strings multi-linha via script
Ao gerar listas de strings pendentes via PowerShell `python -c @"..."@`, um
`'\\\\n'` mal contado no literal Python virou 3 caracteres (`\`, `\`, `n`) em vez
de 2 (`\`, `n`) no arquivo de saída — quebrando o match exato de qualquer entrada
multi-linha na hora de reaplicar a tradução. Fix: normalizar com regex
`re.sub(r'\\+n', '\n', texto)` em vez de `.replace('\\n', '\n')` literal ao
reconstituir os textos.

### 6. Regex direto em `<message>...</message>` sem isolar por bloco corrompe dados
A primeira tentativa de extrair `<source>` + `<translation unfinished>` com um
regex único, sem isolar cada `<message>` primeiro, misturou texto de mensagens
adjacentes quando havia mensagens já traduzidas intercaladas com pendentes.
Fix: **sempre** isolar blocos com `re.findall(r'<message>(.*?)</message>', ..., re.DOTALL)`
primeiro, e só then buscar `<source>`/`<translation>` dentro de cada bloco.

## Onde olhar primeiro na próxima tradução

- `translations/mira_en.ts` — se `grep -c unfinished` voltar > 0, tem trabalho
  pendente. Rodar `lupdate` primeiro pra garantir que não tem string nova.
- Contar `grep -o "tr(\"" src/*.cpp | wc -l` vs mensagens no `.ts` de vez em
  quando — se o código cresceu mais rápido que o `.ts`, tem string sem `tr()` em
  algum lugar (como aconteceu com `ExportPanel.cpp`, `Quotes.cpp`, `MapView.cpp`
  e `GeoData.cpp`, que tinham **zero** `tr()` antes desta rodada).
- Classes locais definidas dentro de `.cpp` (padrão comum neste projeto pra
  widgets pequenos) — checar se têm `Q_OBJECT`. Se não tiverem, qualquer `tr()`
  solto ali dentro é suspeito (ver bug #2).
