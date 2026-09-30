**Qenna Writer 1.4.2**
Un ajuste más en el instalador, pensando en PCs más lentas: ahora solo el propio instalador cierra Qenna si sigue abierto (preguntando antes). Windows ya no interviene, y el mensaje de "no pudo cerrar automáticamente todas las aplicaciones" ya no aparece.

**Qenna Writer 1.4.1**
Un ajuste rápido en el instalador. Si Qenna seguía ejecutándose en segundo plano después de pedirle que se actualizara, la instalación se quedaba trabada en "El instalador no pudo cerrar automáticamente todas las aplicaciones". Ahora el instalador espera a que Qenna termine de cerrarse y, si se quedó colgado, pregunta y lo cierra por ti.
¿Vienes directo de la 0.18? Las novedades de la 1.4 están justo debajo.

---

**Qenna Writer - BIG UPDATE 1.4**

La segunda ola.
Reworks en todo lo que quedó fuera de la última versión y algunos fixes más.
Ahora mismo estamos enfocados en un reajuste completo de Qenna en general. Modernización de paneles, herramientas y otras cosas. Llevando la app por un nuevo camino, con una interfaz más amigable, funcional y bonita.

"¿En serio, otro update tan pronto?"
¡ASÍ ES! AQUÍ NO HAY DESCANSO.

Sí, vamos con la 1.4 y con eso...
¡ES OFICIAL!
QENNA POR FIN **COMPLETÓ** SU FASE BETA.

Qenna dejó atrás hace tiempo esa fase de "prueba" de la versión 0.x.x. La app es estable, funcional y está en un nivel en el que esa numeración ya no tiene sentido.
Así que sí:

¡Llegamos a la 1.0! ¡Y que vengan muchas más!

¡Vamos allá!

## Novedades

**INTRODUCING!**

*THE NEEEW DIALOGUE DETECTOR ENGINE HEAVYWEIGHT CHAMPION!*

**• NUEVO MOTOR DEL DETECTOR DE DIÁLOGOS**
Les presento: **Granna**.
Granna es el nuevo motor de detección de diálogos, escrito desde cero absoluto para las nuevas versiones de Qenna.
El motor de texto antiguo era... problemático. Muchas reglas, muchos errores. Fallaba demasiado, dejaba pasar diálogos obvios y no sabía quién había dicho la mayoría de ellos.

Así que, para acabar con los problemas, hicimos limpieza y lo reconstruimos.
El nuevo motor llega a hasta 3x más aciertos y detecciones que el antiguo.
Y no, no estoy exagerando.

Al final de este patch note dejo más detalles sobre el nuevo motor. Cómo funciona, porcentajes de acierto y todo eso, por si te da curiosidad. Vale la pena leerlo, de verdad. Es bueno saber cómo funciona.

**• Rework: pestaña Diálogos del Pensario**
Junto con Granna, la pestaña de Diálogos del Pensario tiene cara nueva: el elenco arriba (haz clic en una cara para ver solo sus líneas), las líneas en bloques con el color de cada personaje y un botón para cambiar de manuscrito. Las líneas probables aparecen con trazo discontinuo; un clic derecho las confirma o corrige.

**• Rework de Ajustes**
La pestaña de ajustes estaba un poco... desfasada. La app ya tenía muchos ajustes distintos, pero el panel no tenía ninguna estructura ni interfaz real para separar el grano de la paja.
El panel de ajustes pasó por un rework completo. Ahora cada cosa está en su sitio.

**• Rework del panel y del creador de Themes**
Sí, lo tocamos. El panel de Themes pasó por un rework completo, tanto visual como funcional.
La nueva vista previa de Themes es **INCREÍBLE**, con comparación entre themes en tiempo real, navegación más fluida, sugerencias de similares, filtros de novedades y de usados recientemente, y mucho más.
Además, el **Creador de temas** también tiene cara nueva. Una interfaz mucho más fácil de entender, vistas previas más precisas y mucha más libertad para crear el theme que sea la cara de tu proyecto.
Las nuevas opciones incluyen: color de los iconos, paneles de cristal y también el color de los botones dentro de los cajones (antes eran fijos según el color del propio cajón; ahora pueden tomar el color de acento del theme).
Otros puntos destacados: color y fuente del DocHeader (el título del doc que estás editando, arriba del todo) y color y borde propios para cada panel.
Además, me tomé la libertad de marcar algunos themes como recomendados. Themes que considero los mejores que hemos hecho. Llevan un sello y aparecen arriba en su categoría.
El rework también trajo una optimización increíble al panel de Themes. Antes tardaba de 2 a 4 segundos en abrirse; ahora abre en menos de 1 segundo.
Ahora también hay indicación de Themes parecidos, el sistema de Día y noche recibió ajustes visuales y mucho más.

**• Creador de temas: degradado y grano en el fondo**
Esta idea la tomamos prestada de Qenna Cover, nuestro creador de portadas.
Hasta ahora, el fondo de un theme sin foto era un solo color plano en toda la pantalla. En los themes oscuros nadie lo nota, todo es oscuro de todos modos. Pero en los claros el fondo destaca detrás de la página, y un color sólido ahí... no brilla.
Ahora el fondo puede tener un degradado (abajo, arriba, los dos o viñeta), con color, opacidad y tamaño. Y grano. El grano también tiene tamaño: fino, como película de cine, o grande, que se convierte en piedra, fieltro o nácar. También funciona sobre la foto en los estampados.
Está todo en el Creador de temas, en la parte de Fondo.

**• Nuevos Themes:**
Esta tanda se enfocó en themes claros. Y no fue por casualidad: el problema de los claros de Qenna nunca fue la cantidad, era la variedad. Se parecían demasiado.
Así que, esta vez, cada theme usa todo lo que tiene el Creador: color por panel, iconos, título, cajones, degradado y grano. Y cada uno es algo que reconoces al instante.

**Claros|** World 1-1, DMG-01, Solitaire, Ballpoint, Deep End, Clay Court, Alpenglow, Fluorescent, Luciana

Mis destacados: World 1-1 (el primer nivel de Mario, con el bloque ? en el contador) y DMG-01 (la Game Boy original, la carcasa y el marco de la pantalla).
Y Luciana es especial. Está inspirada en mi guitarra, una custom que armé pieza por pieza: golpeador azul perlado, cuerpo crema, herrajes dorados. Quedó como ella se merece.

Pero no solo de themes claros vive el hombre.
También tenemos una nueva tanda de estampados, enfocados en la estética roja.

**Estampados|** Scarlet Ridge, Red Alps, Mars Range, Redemption

Entre ellos, mi pick: **Redemption**.
*for those who stay unshaken amidst a crash of worlds*

Esto también trajo un ajuste. Varios Themes cambiaron de categoría. Algunos eran demasiado coloridos para considerarse oscuros, otros demasiado claros para ser amarillentos o coloridos, etc. El cambio afectó a todas las categorías (excepto los estampados). Así que, si no encuentras un theme que te gustaba y no habías marcado como favorito, ¡usa la búsqueda o explora!

**• Rework del Help Panel**
Siguiendo la línea de los ajustes, el Help Panel también se rehízo. Las secciones antiguas también se actualizaron con las nuevas funciones.

**• Nuevos diseños del cajón de manuscritos**
¡También llegaron! El cajón de manuscritos recibió algunos diseños de UI más. Intercambiables, igual que los anteriores. Además, el botón de nuevo manuscrito tiene un icono nuevo.
Algunos de ellos se centran en la herramienta nueva que trajimos en la versión anterior: las ilustraciones.
Y hablando de eso:
**¡Nuevas ilustraciones de capítulos llegan en esta actualización!**
Cyberpunk, Japón feudal, Ventisca, Steampunk, Medieval y otras.
También ajustamos los colores para que reflejen mejor los colores de las partes dentro del manuscrito. Bonito, elegante.

**• Rework: Nuevo cajón**
Nuevo creador de cajones, más limpio, bonito y rápido. Los mismos cajones, más fáciles de crear.

**• Rework: Glosario**
Ni él se salvó. El glosario también pasó por un rework completo. Tanto su panel como su acceso desde el menú de selección. Y ahora también puede resaltar en el texto las palabras guardadas en él.
Además recibió su propia pestaña en el Pensario, con términos que ganan tipos y otras grafías.

**• Rework: Crear documento a partir del texto**
Pestaña rehecha, más rápida y dinámica.

**• Rework: ventanas**
Adiós a las ventanas de Windows. Todo lo que todavía abría una caja del sistema ahora es una hoja de Qenna. Al menos, todas las que yo sepa. Si encuentras alguna perdida por ahí, ¡avísame!
Esto abarca todo: Creador de Mundos y Sistemas, Pizarra, Grupos, Timeline, etc.

**• Rework de Nuevo capítulo/Nueva escena**
Nuevo popup para nuevo capítulo y nueva escena. Más bonito, para encajar con las actualizaciones de UI.
Esto también incluye algo que les debía desde hace tiempo: un separador de escenas decente. Ahora la app tiene dos opciones de separador, intercambiables en los ajustes. Los dos mucho más bonitos que la línea recta y torcida que teníamos.

**• Rework de Nuevo personaje/Editar personaje**
Sí, ni ese se salvó. Al crear personajes, ahora tienes un panel mucho más bonito para dejar fluir la creatividad.

**• Rework: añadir evento a la Timeline**
El antiguo literalmente abría la Timeline para que crearas el evento allí, arrancándote de tu texto con cierta brutalidad. Ya no.

**• Rework: Nuevo proyecto**
La ventana de nuevo proyecto tampoco se salvó: más bonita y fácil de rellenar. Ahora está todo en una sola hoja, sin los 3 pasos de antes.

*Y ahora...*
*Chicos... respiren hondo...*
**HE'S BACK**
**• ¡Creador de portadas!**
Bueno, al menos una versión más pequeña y tranquila, pero volvió. El creador de portadas nativo, como teníamos en el difunto Mira Writing.
Funciona bien, ideal para crear portadas rápidas y no tener excusa para quedarte con un cuadrado negro con el nombre de tu libro en el Main Menu.
Me aseguré de que tuviera varias opciones útiles y necesarias para crear al menos portadas decentes.

El nuevo creador de portadas no es tan profundo como Qenna Cover (nuestro otro proyecto), y tampoco lo intenta. Su único objetivo es que tu proyecto se vea más bonito en el Menú sin que tengas que descargar otro programa para eso.
Pero Qenna Cover sigue disponible... medio muerto, sí. Pero sigue vivo y pienso actualizarlo pronto. Así que estén atentos.

De todos modos, nuestro Cover-Mini es justo y te va a dar resultados satisfactorios. Les va a gustar.
¿Y sinceramente? No es tan profundo como Qenna Cover, pero tampoco se queda tan lejos.

Y claro, lo clásico: puedes exportar tus portadas y usarlas en otros lugares, como EPUBs o sitios web.

**• Función: Eventos de la Timeline**
La opción de los ajustes para crear varios eventos de la Timeline a la vez se movió dentro del panel de Timeline, y también pasó por un rework visual completo.

**• Aviso de actualización en el menú**
¿Salió una versión nueva? Ahora el aviso aparece en grande, arriba del menú principal, con las novedades y el progreso de la descarga.

*Y ahora, antes de pasar a los fixes y ajustes, volvamos a nuestro nuevo detector de diálogos, Granna.*

**• Granna: ¿Cómo funciona?**

Primero hay que hablar del detector antiguo, para que se entienda la diferencia.
El detector antiguo funcionaba así:
Tomaba cada línea aislada y buscaba un nombre de personaje después de la raya (o después de las comillas).

○ Encontraba un nombre: la línea era de ese personaje.
○ No encontraba ninguno: se rendía. "Sin atribuir."
○ Encontraba dos: también se rendía. Y eso pasaba mucho, porque confundía la continuación de la línea con la acotación:

Así:

—Él no —admitió María—. Pero Samantha sí.

○ Veía "María" y "Samantha" y tiraba la toalla.

"—No voy —dijo ella." Sin nombre, sin hablante.
"—¿Por fuera?" Línea sin acotación, sin hablante.

Si el proyecto tenía narrador, cualquier línea con "yo", "dijo" o "como" se iba al narrador, aunque fuera de otra persona.

Solo entendía rayas y comillas dobles. Las comillas « », comunes en italiano y francés, ni las veía.

*Y ahora, amigos... abróchense el cinturón.*

**ASÍ ES COMO TRABAJA GRANNA:**

Lee la escena de arriba abajo, recordando la conversación:

**○ Separa lo que es diálogo de lo que es narración.**
"admitió María" es la acotación; "Pero Samantha sí" es diálogo.
**○ Entiende quién es el sujeto.**
En "Clara dijo", quien habla es Clara. En "miró a Clara", no.
**○ Sigue los turnos.**
Si Clara y João están conversando, "—¿Por fuera?" viene justo después de Clara, así que es de João.
**○ Se da cuenta de a quién llamaron.**
En "—Gracias, María.", quien habla no es María: es quien está hablando con ella.
**○ Entiende "dijo él" y "dijo ella"**
por el género de cada personaje. **Lo marcas en la nueva Forma de tratamiento al crear/editar personajes**, o lo deduce del texto.
**○ Reconoce a los figurantes.**
"—Sala 9 —dijo el chico." Hay hablante, pero no es nadie de tu elenco, así que no le da la línea al personaje equivocado.
**○ Entiende la primera persona**
por el narrador marcado en el capítulo: "—No —respondí."
**○ Entiende el gesto antes de la línea**
En libros con comillas: Oda se encogió de hombros. "La justicia no tiene nada que ver."
**○ Descubre solo el idioma de cada capítulo**
Y usa las reglas de esa lengua: raya, comillas, « », la raya del español y el inciso del francés ("— Je pars, dit Marie.").
**○ En guion**
Lee el hablante directamente del bloque de personaje.

**○ Y te dice cuándo está adivinando**
Con el nombre escrito en la acotación, la línea se marca como segura. Cuando lo deduce por la conversación, aparece como "probable", y un clic derecho la confirma o corrige. Lo que corriges nunca se deshace.

**○ Por detrás, las líneas guardadas también son fiables**

¿Editaste una línea? La versión vieja ya no se queda olvidada en el archivo sumando en las estadísticas.
Dos líneas iguales ("—Sí.") en el mismo capítulo siguen siendo dos.
"No es diálogo" quita para siempre una línea que no es diálogo; antes volvía sola.
Escanear todo muestra, en la barra, cada línea nueva, cada cambio de hablante y cada línea que salió, con dónde está y quién la dijo.

**TASAS DE ACIERTO:**
En capítulos de prueba en los cinco idiomas:
motor antiguo 25% | Granna 93%, llegando al 97% con la forma de tratamiento marcada en los personajes.
Por idioma (Granna): IT 100% · PT-BR 100% · ES 92% · FR 91% · EN 81% (100% con forma de tratamiento)

La mejor tasa del motor antiguo era del 54%, en ES.
El italiano ni lo detectaba.
9% en FR.
25% en EN.

No hace falta alargarlo. Es un salto de potencia absurdo.

*Y ahora, sigamos con el viaje...*

## Fixes
Algunos fixes rápidos para pulir algunas aristas de la última versión.
Fue un update grande, así que era de esperar que algo se escapara. Sin drama, vamos a lo que se arregló.

**• Error en la actualización automática**
*\\ no se especificó la ruta.*
Este error era Qt escapando unas comillas. El cmd no interpreta el carácter, así que la ejecución del update fallaba.
Se corrigió en un re-release de la 0.18.0. Edité aquella release con un setup que ya tenía la corrección. Así que si la descargaste después, probablemente no te pase.
Corregido.

**• El porcentaje de la meta diaria se quedaba en 100%, aunque siguieras**
Sí. Los nuevos contadores son preciosos, pero tenían un problema de visualización en el porcentaje de la meta, que se congelaba al alcanzarla y dejaba de crecer.
Corregido.

**• Color de los Themes x Nuevos estilos de visualización**
El color de algunos themes podía dejar ilegible parte del contenido en los nuevos estilos de visualización del Pensario. Corregido.

**• Color de los tooltips de la app**
El tooltip del cajón de manuscritos y muchos otros podían quedarse totalmente negros, sin contenido legible. Corregido.

**• Ajuste en el contador**
Ahora se anima al cambiar de tamaño.

**• Crash al cambiar el género (M/F) de los vínculos**
Al crear un vínculo, si intentabas cambiar el género de las opciones, el programa se cerraba en seco.
Corregido.

**• Menú principal de dos ventanas**
Antes, al ir al menú principal, Qenna se dividía en dos ventanas, una con el editor y otra con el menú.
Ahora es una sola.

**• Otros fixes**
Cerrar, Cancelar y el menú de copiar/pegar ahora aparecen en el idioma de la app.
Se corrigieron etiquetas cortadas en la ficha del territorio (en inglés) y en el chip del glosario.

**• Colores de las ventanas**
Heredaban el color de Windows. Fixed. Ahora las ventanas de Qenna toman el color del Theme.

**• Ventana del Patch Note**
Aaah, por fin corregí el margen de la ventana del patch note. Esta vez debería verse bien.

---
Eso es todo, mis queridos. Diviértanse.
Qenna está realmente SÓLIDO, en su mejor momento. Bonito, fluido, funcional y un gusto de usar.

*Arrepintiéndome de estar dando esto gratis, pero no voy a echarme atrás,*
P.H. Lobato — Guardián de las Tierras de Qenna
