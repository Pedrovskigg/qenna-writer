**PATCH NOTE - MAJOR UPDATE - 0.17.0**

Aqui estamos! Escrevendo mais um belo patch note que ninguém lerá.

Antes de começar esse patch note, gostaria de pedir um minuto de silêncio.
Uma perda dolorosa e inestimável.
Recentemente, devido uma falha minha, acabei excluindo **TODOS** os meus projetos do Qenna. Trabalhos de meses, milhões de caracteres que simplesmente deixaram de existir. Sem possibilidade de recuperação.
Admito que isso realmente me derrubou e até devorou toda a motivação para escrever, ao menos por um bom tempo.

Com essa dolorosa perda, veio a inspiração para algumas novas funções. Para garantir que ninguém jamais sofra o que eu sofri.

## Novidades

Tudo que temos de novo aqui.

**• Novas traduções:**
Italiano (IT) e Francês (FR)
Não sou fluente nesses idiomas. Me avisem se encontrarem algum erro de tradução, por favor!

**• Função backups**
Sim, o Qenna tecnicamente já tinha backups. Mas apenas de arquivos básicos do projeto que seriam úteis em perdas relativas, mas não totais como no meu caso.
Agora, você pode configurar o Qenna para realizar backups completos. O app literalmente criará um .zip do seu projeto inteiro e salvará na pasta que você quiser, incluindo nuvens.
Você pode configurar de quanto em quanto tempo esse backup ocorre ou, caso prefira fazê-lo manualmente, configurar lembretes para realizá-lo.
Não deixe que a vida tire tudo de você como tirou de mim. O universo é traiçoeiro e a tecnologia é o seu lacaio.

**• Função lixeira**
Além disso, agora o app não exclui de forma direta os projetos. Não, senhor. O Qenna agora cria uma lixeira temporária para onde os projetos excluídos através do app vão. Essa lixeira pode ser acessada e usada para restaurar projetos deletados.
O acesso para ela se encontra no main menu.

**• Exportação do Bible**
Uma extensão da ferramenta de exportação.
Agora, é possível exportar o universo inteiro do seu projeto em um documento só: gavetas, fichas, vínculos, glossário, territórios do Criador de Mundos, Sistemas e até locais marcados no mapa-múndi. Sai em PDF, DOCX ou ODT, pra consultar sem nem abrir o app.

**• Busca dentro da gaveta**
É possível realizar buscas dentro das gavetas agora. A busca varre a gaveta inteira, subpastas incluídas, pelo título ou pelo conteúdo dos documentos. Aquele personagem perdido numa pasta que você nem lembra qual é? Achado.

**• Expansão de suporte a manuscritos múltiplos**
Algumas ferramentas ainda só conseguiam acessar conteúdos existentes no primeiro manuscrito do projeto. Não mais. Todos os manuscritos do projeto agora podem ser usados pelas ferramentas.

**• Trava de segurança: pastas do sistema**
Essa aqui é pessoal. O Qenna agora se recusa a usar pastas como Documentos, Área de Trabalho, a sua pasta pessoal ou a raiz do disco como pasta de projeto — e também se recusa a excluí-las. Foi exatamente assim que eu perdi tudo. Não vai acontecer com vocês.

**• Personalização: As Barras**
Eu tive essa ideia no nosso finado Mira Writing. O conceito de personalização era que pudéssemos mudar as barras do app de lugar. Colocar a barra de documentos do outro lado, a top bar na direita. E por aí vai. O problema é que, quando eu tive essa ideia, a posição das barras já era intrínseca no código e muitas funções dependiam delas, o que impossibilitou a ideia. Mas não dessa vez.
Agora, através das configurações, você pode alterar a posição das barras do app. Colocando-as onde quiser.

**• Personalização: Botões**
Os botões da barra de ferramentas agora podem ser movidos. Basta clicar e segurar em um botão para movê-lo para onde quiser. Você também pode mover grupos inteiros de botões (ex: as opções do editor) para outro lugar. Deixando a barra do jeito que desejar.

**• Personalização: Nova opção na criação de Themes**
Radius dos painéis e barras.
Mais quadrados, mais redondos, como prefere? Agora você pode customizar na opção de criação de Themes.

**• NOVA UI**
Tecnicamente.
A UI do Qenna passou por um retrabalho. Nada exagerado, mas... diferente.
O Qenna agora tem uma interface mais reduzida e focada, que ouso dizer que até lembra a primeira versão do app (Mira Writing). Dá até uma nostalgia.

Essa atualização também se expande ao menu principal. Trazendo novas logos, um Q animado que mostra um mundo diferente para cada gênero e uma nova splash de início do app.

**• Função de Renomear Personagens**
Agora, se você editar um personagem e mudar o nome dele nos metadados (clique direito, editar metadados), será chamada uma janela para correção completa. O nome do personagem mudará em TODOS os capítulos dos manuscritos e documentos das gavetas. Além de menções, variações de cena, fichas, falas salvas no Pensário, cards da Lousa, pins do mapa e menções do Construtor. Basicamente cobre o app inteiro.

**• Dicionário de sinônimos:**
Agora, você pode consultar sinônimos das palavras, já no tempo verbal certo pra encaixar na frase. O português já vem com o app; nos outros idiomas, o dicionário é baixado automaticamente assim que você escolhe a língua do app. Downloads adicionais só são necessários se você escrever em uma língua diferente da do app.

**• Modo TTS (Text to Speech)**
Atrasado, eu sei. Mas chegou a tempo. Agora, você pode selecionar trechos do seu projeto para ler em voz alta.

**• Exportação: modo submissão**
Modo de exportação que já formata o seu manuscrito para trabalhos editoriais — como envio para editoras, revisões e etc. Opcional na janela de exportação e funcional em PDF, DOCX e ODT (no EPUB não faz sentido, já que e-book não tem página fixa). Pra enviar pra editora de verdade, prefira DOCX ou PDF: o ODT não leva o cabeçalho de página.

**• Detector de Repetições**
O detector de repetições pode ser ativado na barra superior. Com ele, o app pode detectar palavras repetidas no texto e grifá-las, além de também indicar parágrafos longos demais que podem prejudicar o ritmo do seu texto, ajudando a refinar sua escrita.
Ele também pega pronome repetido demais ("ela... ela... ela") e aquele acúmulo de advérbios terminados em -mente. Funciona em português, espanhol, italiano e francês. No inglês, só palavras idênticas — a língua deles não colabora.

## Fixes

O que arrumamos:

**• Exportação de PDF corrigida.**
Havia um bug na exportação para PDF que fazia as exportações pesarem até centenas de megabytes dependendo da fonte utilizada no documento. Basicamente, a exportação para PDF não usava a fonte direto no texto, e sim redesenhava o texto vetorialmente por algum motivo.
Corrigido. Os PDFs agora devem ficar bem mais leves e rápidos para exportar.

**• Mudança de motor**
O Qenna antes usava o motor DirectWrite para desenho das fontes e textos. E era isso que causava o bug de pdf que tínhamos acima. Agora, o Qenna utiliza o FreeType. Um motor mais direto e que teve um impacto ABSURDO na performance do app em textos. Não que o Qenna rodasse mal antes, sempre funcionou muito bem. Mas vocês vão sentir uma diferença absurda na resposta do editor durante a escrita.

**• Corrigida exportação de EPUB**
Corrigido um problema na exportação de epub, no qual a língua do e-book era sempre considerada português, independente do idioma em que o livro foi escrito. Isso era devido ao fato que o idioma base do app é português e a exportação não levava em consideração o idioma do texto. Agora o epub segue o idioma do corretor do seu projeto — livro escrito em italiano sai como italiano, com sumário e tudo. Corrigido.

**• Correções de tradução**
Corrigidas algumas pontas soltas de tradução na área do World Builder e Painel de Exportação.

**• Compactação de sons**
As músicas do Som Imersivo receberam compactação agora, e as imagens dos Themes estampados foram redimensionadas. Sem perder nada.
O resultado: o instalador muito mais leve. Indo de \~660mb de antes para algo em torno de 170mb.

---

Por enquanto é só, amigos. Espero que aproveitem essa nova versão e se divirtam.
Admito que está bem interessante. A cada atualização, a cada trabalho, eu vejo o Qenna ganhando cada vez mais forma, mais identidade e se tornando o que foi pensado para ser: uma ferramenta criativa de ponta.

Lembrando sempre que: caso você goste do Qenna, considere compartilhá-lo! Envie para outros autores que conhece ou também, pode nos ajudar muito com avaliações positivas no [AlternativeTo](https://alternativeto.net/software/qenna-writer/about/), para que o Qenna alcance cada vez mais pessoas.

Amo vocês!

Cafeínado,
— P.H. Lobato, Guardião das Terras de Qenna
