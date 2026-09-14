**PATCH NOTE - MAJOR UPDATE - 0.17.0**

Here we are! Writing yet another lovely patch note that nobody will read.

Before starting this patch note, I'd like to ask for a minute of silence.
A painful, priceless loss.
Recently, through a mistake of my own, I ended up deleting **ALL** of my Qenna projects. Months of work, millions of characters that simply stopped existing. No chance of recovery.
I'll admit it really knocked me down and even devoured all my motivation to write, at least for a good while.

With that painful loss came the inspiration for a few new features. To make sure nobody ever goes through what I went through.

## What's new

Everything we've got that's new.

**• New translations:**
Italian (IT) and French (FR)
I'm not fluent in these languages. Please let me know if you find any translation mistakes!

**• Backups**
Yes, Qenna technically already had backups. But only of basic project files, which were useful for partial losses, not total ones like mine.
Now you can set Qenna up to make full backups. The app will literally create a .zip of your entire project and save it to whatever folder you want, cloud folders included.
You can set how often the backup runs or, if you'd rather do it by hand, set up reminders to do it.
Don't let life take everything from you like it took from me. The universe is treacherous, and technology is its henchman.

**• Trash**
On top of that, the app no longer deletes projects outright. No, sir. Qenna now keeps a temporary trash where projects deleted through the app go. You can open that trash and use it to restore deleted projects.
You'll find it in the main menu.

**• Universe Bible export**
An extension of the export tool.
You can now export your project's entire universe into a single document: drawers, character sheets, links, glossary, World Builder territories, Systems and even places marked on the world map. It comes out as PDF, DOCX or ODT, so you can look things up without even opening the app.

**• Search inside drawers**
You can now search inside drawers. The search sweeps the whole drawer, subfolders included, by title or by document content. That character lost in a folder you can't even remember? Found.

**• Wider support for multiple manuscripts**
Some tools could still only reach content from the project's first manuscript. Not anymore. Every manuscript in the project can now be used by the tools.

**• Safety lock: system folders**
This one's personal. Qenna now refuses to use folders like Documents, Desktop, your user folder or the root of a drive as a project folder — and it also refuses to delete them. That's exactly how I lost everything. It won't happen to you.

**• Customization: The Bars**
I had this idea back in our late Mira Writing. The idea was that we could move the app's bars around. Put the documents bar on the other side, the top bar on the right. And so on. The problem was that, when I had the idea, the bars' positions were already baked into the code and lots of features depended on them, which made it impossible. But not this time.
Now, through the settings, you can change where the app's bars go. Put them wherever you want.

**• Customization: Buttons**
Toolbar buttons can now be moved. Just click and hold a button to move it wherever you want. You can also move whole groups of buttons (e.g. the editor options) somewhere else. Leaving the bar exactly how you like it.

**• Customization: New option when creating Themes**
Panel and bar radius.
More square, more round, what's your style? You can now customize it when creating Themes.

**• NEW UI**
Technically.
Qenna's UI got a rework. Nothing over the top, but... different.
Qenna now has a leaner, more focused interface that, dare I say, even reminds me of the app's first version (Mira Writing). Kind of nostalgic.

This update also reaches the main menu. Bringing new logos, an animated Q that shows a different world for each genre, and a new startup splash.

**• Character renaming**
Now, if you edit a character and change their name in the metadata (right click, edit metadata), a window will pop up for a full correction. The character's name will change in ALL manuscript chapters and drawer documents. Plus mentions, scene variations, character sheets, lines saved in the Pensarium, Board cards, map pins and Builder mentions. It basically covers the whole app.

**• Thesaurus:**
You can now look up synonyms for words, already in the right verb tense to fit the sentence. Portuguese ships with the app; for the other languages, the thesaurus downloads automatically as soon as you pick the app's language. Extra downloads are only needed if you write in a language different from the app's.

**• TTS mode (Text to Speech)**
Late, I know. But it made it in time. You can now select passages of your project to be read aloud.

**• Export: submission mode**
An export mode that formats your manuscript for editorial work — like sending it to publishers, revisions and so on. Optional in the export window and works with PDF, DOCX and ODT (it makes no sense for EPUB, since e-books don't have fixed pages). To actually send it to a publisher, go with DOCX or PDF: ODT doesn't carry the page header.

**• Repetition Detector**
The repetition detector can be turned on from the top bar. With it, the app can detect repeated words in your text and highlight them, and also flag paragraphs that are too long and might hurt your text's rhythm, helping you polish your writing.
It also catches pronouns repeated too often ("she... she... she") and that pile-up of adverbs ending in -mente. It works in Portuguese, Spanish, Italian and French. In English, only identical words — the language just doesn't cooperate.

## Fixes

What we fixed:

**• PDF export fixed.**
There was a bug in PDF export that could make exports weigh up to hundreds of megabytes depending on the font used in the document. Basically, PDF export wasn't using the font directly in the text, it was redrawing the text as vectors for some reason.
Fixed. PDFs should now be much lighter and faster to export.

**• Engine change**
Qenna used to draw fonts and text with the DirectWrite engine. And that was what caused the PDF bug above. Qenna now uses FreeType. A more direct engine that had an ABSURD impact on the app's text performance. Not that Qenna ran badly before, it always worked really well. But you'll feel an absurd difference in how the editor responds while you write.

**• EPUB export fixed**
Fixed a problem in EPUB export where the e-book's language was always set to Portuguese, no matter what language the book was written in. That was because the app's base language is Portuguese and the export didn't take the text's language into account. Now the EPUB follows your project's spell-checker language — a book written in Italian comes out as Italian, table of contents and all. Fixed.

**• Translation fixes**
Fixed some loose translation ends in the World Builder and the Export Panel.

**• Sound compression**
The Immersive Sound tracks are now compressed, and the images of the patterned Themes were resized. Without losing anything.
The result: a much lighter installer. Going from \~660 MB before to somewhere around 170 MB.

---

That's all for now, folks. I hope you enjoy this new version and have fun with it.
I'll admit it's looking pretty interesting. With every update, with every bit of work, I see Qenna taking more and more shape, more identity, and becoming what it was meant to be: a cutting-edge creative tool.

And always remember: if you like Qenna, consider sharing it! Send it to other authors you know, or you can also help us a lot with positive reviews on [AlternativeTo](https://alternativeto.net/software/qenna-writer/about/), so Qenna can reach more and more people.

Love you all!

Caffeinated,
— P.H. Lobato, Guardian of the Lands of Qenna
