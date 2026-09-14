**PATCH NOTE - MAJOR UPDATE - 0.17.0**

¡Aquí estamos! Escribiendo otro precioso patch note que nadie leerá.

Antes de empezar este patch note, me gustaría pedir un minuto de silencio.
Una pérdida dolorosa e inestimable.
Hace poco, por un error mío, terminé borrando **TODOS** mis proyectos de Qenna. Meses de trabajo, millones de caracteres que simplemente dejaron de existir. Sin posibilidad de recuperación.
Admito que eso me derrumbó de verdad y hasta devoró todas mis ganas de escribir, al menos durante un buen tiempo.

Con esa dolorosa pérdida llegó la inspiración para algunas funciones nuevas. Para asegurar que nadie sufra jamás lo que yo sufrí.

## Novedades

Todo lo nuevo que tenemos aquí.

**• Nuevas traducciones:**
Italiano (IT) y Francés (FR)
No hablo esos idiomas con fluidez. ¡Avísenme si encuentran algún error de traducción, por favor!

**• Copias de seguridad**
Sí, técnicamente Qenna ya tenía copias de seguridad. Pero solo de los archivos básicos del proyecto, útiles en pérdidas parciales, pero no totales como en mi caso.
Ahora puedes configurar Qenna para hacer copias de seguridad completas. La app literalmente creará un .zip de tu proyecto entero y lo guardará en la carpeta que quieras, nubes incluidas.
Puedes configurar cada cuánto tiempo se hace la copia o, si prefieres hacerla a mano, configurar recordatorios para hacerla.
No dejes que la vida te lo quite todo como me lo quitó a mí. El universo es traicionero y la tecnología es su lacayo.

**• Papelera**
Además, ahora la app no borra los proyectos de forma directa. No, señor. Qenna ahora crea una papelera temporal a donde van los proyectos borrados desde la app. Esa papelera se puede abrir y usar para restaurar proyectos eliminados.
El acceso está en el menú principal.

**• Exportación de la Biblia del universo**
Una extensión de la herramienta de exportación.
Ahora es posible exportar el universo entero de tu proyecto en un solo documento: cajones, fichas, vínculos, glosario, territorios del Creador de Mundos, Sistemas y hasta lugares marcados en el mapamundi. Sale en PDF, DOCX u ODT, para consultarlo sin siquiera abrir la app.

**• Búsqueda dentro del cajón**
Ahora se puede buscar dentro de los cajones. La búsqueda recorre el cajón entero, subcarpetas incluidas, por el título o por el contenido de los documentos. ¿Ese personaje perdido en una carpeta que ni recuerdas cuál es? Encontrado.

**• Más soporte para varios manuscritos**
Algunas herramientas todavía solo llegaban al contenido del primer manuscrito del proyecto. Ya no. Todos los manuscritos del proyecto ahora pueden usarse en las herramientas.

**• Seguro: carpetas del sistema**
Esta es personal. Qenna ahora se niega a usar carpetas como Documentos, Escritorio, tu carpeta personal o la raíz del disco como carpeta de proyecto — y también se niega a borrarlas. Fue exactamente así como lo perdí todo. No les va a pasar a ustedes.

**• Personalización: Las Barras**
Tuve esta idea en nuestro difunto Mira Writing. La idea era que pudiéramos cambiar las barras de la app de lugar. Poner la barra de documentos del otro lado, la barra superior a la derecha. Y así. El problema es que, cuando tuve la idea, la posición de las barras ya estaba metida en el código y muchas funciones dependían de ella, lo que la hizo imposible. Pero esta vez no.
Ahora, desde los ajustes, puedes cambiar la posición de las barras de la app. Poniéndolas donde quieras.

**• Personalización: Botones**
Los botones de la barra de herramientas ahora se pueden mover. Solo haz clic y mantén presionado un botón para moverlo adonde quieras. También puedes mover grupos enteros de botones (ej.: las opciones del editor) a otro lugar. Dejando la barra como más te guste.

**• Personalización: Nueva opción al crear Temas**
Radio de los paneles y barras.
¿Más cuadrados, más redondos, cómo los prefieres? Ahora puedes personalizarlo al crear Temas.

**• NUEVA UI**
Técnicamente.
La UI de Qenna pasó por una renovación. Nada exagerado, pero... diferente.
Qenna ahora tiene una interfaz más reducida y enfocada que, me atrevo a decir, hasta recuerda a la primera versión de la app (Mira Writing). Da hasta nostalgia.

Esta actualización también llega al menú principal. Con logos nuevos, una Q animada que muestra un mundo diferente para cada género y una nueva pantalla de inicio de la app.

**• Renombrar personajes**
Ahora, si editas un personaje y cambias su nombre en los metadatos (clic derecho, editar metadatos), se abrirá una ventana para una corrección completa. El nombre del personaje cambiará en TODOS los capítulos de los manuscritos y documentos de los cajones. Además de menciones, variaciones de escena, fichas, frases guardadas en el Pensario, tarjetas de la Pizarra, pines del mapa y menciones del Constructor. Básicamente cubre la app entera.

**• Diccionario de sinónimos:**
Ahora puedes consultar sinónimos de las palabras, ya en el tiempo verbal correcto para encajar en la frase. El portugués ya viene con la app; en los otros idiomas, el diccionario se descarga automáticamente en cuanto eliges el idioma de la app. Solo hacen falta descargas adicionales si escribes en un idioma distinto al de la app.

**• Modo TTS (Text to Speech)**
Tarde, lo sé. Pero llegó a tiempo. Ahora puedes seleccionar fragmentos de tu proyecto para leerlos en voz alta.

**• Exportación: modo envío**
Un modo de exportación que ya formatea tu manuscrito para trabajos editoriales — como envíos a editoriales, revisiones, etc. Opcional en la ventana de exportación y funciona en PDF, DOCX y ODT (en EPUB no tiene sentido, ya que un e-book no tiene página fija). Para enviarlo de verdad a una editorial, mejor DOCX o PDF: el ODT no lleva el encabezado de página.

**• Detector de Repeticiones**
El detector de repeticiones se puede activar en la barra superior. Con él, la app puede detectar palabras repetidas en el texto y subrayarlas, además de señalar párrafos demasiado largos que pueden perjudicar el ritmo de tu texto, ayudándote a pulir tu escritura.
También detecta pronombres repetidos de más ("ella... ella... ella") y esa acumulación de adverbios terminados en -mente. Funciona en portugués, español, italiano y francés. En inglés, solo palabras idénticas — su idioma no colabora.

## Correcciones

Lo que arreglamos:

**• Exportación a PDF corregida.**
Había un bug en la exportación a PDF que hacía que los archivos llegaran a pesar cientos de megabytes según la fuente usada en el documento. Básicamente, la exportación a PDF no usaba la fuente directamente en el texto, sino que redibujaba el texto como vectores por algún motivo.
Corregido. Los PDF ahora deberían quedar mucho más livianos y exportarse más rápido.

**• Cambio de motor**
Qenna antes usaba el motor DirectWrite para dibujar las fuentes y los textos. Y eso era lo que causaba el bug del PDF de arriba. Ahora Qenna usa FreeType. Un motor más directo que tuvo un impacto ABSURDO en el rendimiento de la app con textos. No es que Qenna funcionara mal antes, siempre funcionó muy bien. Pero van a sentir una diferencia absurda en la respuesta del editor mientras escriben.

**• Exportación a EPUB corregida**
Se corrigió un problema en la exportación a EPUB en el que el idioma del e-book siempre se consideraba portugués, sin importar el idioma en que se escribió el libro. Eso pasaba porque el idioma base de la app es el portugués y la exportación no tenía en cuenta el idioma del texto. Ahora el EPUB sigue el idioma del corrector de tu proyecto — un libro escrito en italiano sale como italiano, con índice y todo. Corregido.

**• Correcciones de traducción**
Se corrigieron algunos cabos sueltos de traducción en el Creador de Mundos y en el Panel de Exportación.

**• Compresión de sonidos**
Las pistas del Sonido Inmersivo ahora están comprimidas, y las imágenes de los Temas estampados se redimensionaron. Sin perder nada.
El resultado: un instalador mucho más liviano. Pasando de \~660 MB a algo alrededor de 170 MB.

---

Por ahora eso es todo, amigos. Espero que disfruten esta nueva versión y se diviertan.
Admito que está quedando muy interesante. Con cada actualización, con cada trabajo, veo a Qenna tomando cada vez más forma, más identidad y convirtiéndose en lo que fue pensado para ser: una herramienta creativa de primer nivel.

Y recuerden siempre: si les gusta Qenna, ¡consideren compartirlo! Envíenlo a otros autores que conozcan o, también, pueden ayudarnos mucho con reseñas positivas en [AlternativeTo](https://alternativeto.net/software/qenna-writer/about/), para que Qenna llegue cada vez a más personas.

¡Los quiero!

Cafeinado,
— P.H. Lobato, Guardián de las Tierras de Qenna
