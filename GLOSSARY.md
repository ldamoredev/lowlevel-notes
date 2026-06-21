# Guía de traducción ES — glosario y convenciones

Anchor de estilo y terminología para traducir las notas del atlas al español. Leé
esto antes de traducir y respetalo en **todas** las notas para mantener consistencia.
El objetivo: que un overlay ES se lea como notas de estudio de un ingeniero de
sistemas rioplatense — técnico, denso, directo — no como una traducción literal.

## Arquitectura de la traducción (importante)

Cada nota canónica vive en `content/en/lowlevel/<rama>/<slug>.md` y su overlay español
va en `content/es/lowlevel/<rama>/<slug>.md` (**misma ruta, mismo nombre de archivo**).

- El overlay es el markdown **completo** de la nota (frontmatter + cuerpo), traducido.
- El build (`build.py`) regenera todo el chrome (sidebar, breadcrumbs, prev/next,
  related, TOC, meta-chips) — **eso no se traduce ni va en el overlay**.
- Si falta el overlay, la página `/es/` cae al inglés con un banner. Por eso traducir
  es **incremental y seguro**: una rama a la vez.
- El build mergea el frontmatter EN con el del overlay, así que el overlay solo
  necesita pisar `title` y `description`; igual conviene copiar el frontmatter entero.

Flujo por rama:
1. Listá las notas EN: `ls content/en/lowlevel/<rama>/`.
2. Por cada `<slug>.md`, creá `content/es/lowlevel/<rama>/<slug>.md` con la **misma
   estructura** (mismos headings, mismas tablas, mismos wikilinks).
3. `python3 build.py` y verificá `(unresolved links: 0)`; mirá la página en `/es/`.

## Registro

- **Español rioplatense / voseo.** Imperativos en vos: `Usá`, `Pensá`, `Tené en
  cuenta`, `Fijate`, `Compilá`, `Corré`. Nunca tú/usted.
- **Técnico y denso.** Mismo nivel de detalle que el EN; no suavizar ni inflar.
- **Code, identifiers y flags quedan en inglés** y en `monospace`: `malloc`, `free`,
  `void*`, `-O2`, `rsp`, `mmap`. No traducir nombres de funciones, registros ni flags.

## Terminología (EN → ES)

Mantené el término inglés cuando es el de uso real en la industria; traducí cuando hay
un término castellano natural y establecido. Tabla de referencia:

| EN | ES (preferido) | Nota |
|---|---|---|
| pointer | puntero | |
| pointer arithmetic | aritmética de punteros | |
| stack | stack | "pila" solo si el contexto lo pide; default: stack |
| heap | heap | no "montículo" |
| stack frame | stack frame | |
| undefined behavior (UB) | undefined behavior (UB) | dejar en inglés; aclarar "comportamiento indefinido" la 1ª vez |
| allocator | allocator | "asignador" suena raro; dejar allocator |
| arena / pool / bump allocator | arena / pool / bump allocator | nombres propios, en inglés |
| garbage collector (GC) | garbage collector (GC) | |
| memory leak | memory leak / fuga de memoria | cualquiera; consistencia por nota |
| use-after-free | use-after-free | en inglés |
| buffer overflow | buffer overflow | en inglés |
| alignment / padding | alineación / padding | |
| word / byte / bit | word / byte / bit | "palabra" solo si es claro |
| endianness | endianness | aclarar "orden de bytes" la 1ª vez |
| two's complement | complemento a dos | |
| linker / loader | linker / loader | "enlazador" no |
| linking (static/dynamic) | linking (estático/dinámico) | |
| symbol | símbolo | |
| object file | object file / archivo objeto | |
| calling convention | calling convention | aclarar "convención de llamada" la 1ª vez |
| register | registro | |
| instruction set (ISA) | instruction set (ISA) | |
| syscall | syscall | "llamada al sistema" la 1ª vez |
| process / thread | proceso / thread | "hilo" opcional, default thread |
| file descriptor | file descriptor | |
| signal | señal | |
| race condition / data race | race condition / data race | en inglés |
| atomics | atomics | |
| memory ordering | memory ordering | aclarar "ordenamiento de memoria" la 1ª vez |
| lock-free | lock-free | |
| false sharing | false sharing | |
| cache line / cache miss | cache line / cache miss | |
| benchmarking | benchmarking | |
| bootloader | bootloader | |
| kernel | kernel | |
| paging / virtual memory | paginación / memoria virtual | |
| interrupt | interrupción | |
| scheduler | scheduler | "planificador" opcional |
| driver | driver | |
| toolchain | toolchain | |
| cross-compiler | cross-compiler | |
| compiler output | salida del compilador | |
| RAII | RAII | nombre propio, en inglés |
| smart pointer | smart pointer | |
| move semantics | move semantics | |
| template | template | no "plantilla" en contexto C++ |
| zero-cost abstraction | abstracción de costo cero | |
| craftsmanship | craftsmanship | nombre del movimiento, en inglés |
| test double / seam | test double / seam | en inglés |
| fuzzing | fuzzing | |

## Reglas finas

- **Primera mención bilingüe.** La primera vez que aparece un término que dejás en
  inglés, dale la aclaración entre paréntesis: "undefined behavior (comportamiento
  indefinido)". Después, solo el término inglés.
- **No traduzcas los wikilinks ni los slugs.** El target de `[[lowlevel/<rama>/<slug>|
  Label]]` queda igual; solo traducí el `Label`.
- **Mantené los `## headings` espejados** con el EN (mismo orden y cantidad) para que
  el TOC y los anchors coincidan.
- **Citas y nombres de fuentes** (libros, papers, specs) quedan en su idioma original.
