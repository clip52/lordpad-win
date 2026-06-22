# CSS Cheatsheet

## Seletores
```css
*               /* tudo */
.classe         /* class="classe" */
#id             /* id="id" */
div p           /* p descendente de div */
div > p         /* p filho direto */
div + p         /* p irmão imediato após div */
div ~ p         /* todos p irmãos após div */
a[href]         /* tem atributo */
a[href^="http"] /* começa com */
a[href$=".pdf"] /* termina com */
a[href*="foo"]  /* contém */
input[type="checkbox"]
```

## Pseudo-classes / pseudo-elementos
```css
:hover :focus :active :visited
:focus-visible           /* só foco por teclado */
:first-child :last-child :nth-child(2n)
:nth-child(odd) :nth-of-type(3)
:not(.skip)
:is(h1, h2, h3)          /* agrupa */
:where(h1, h2)           /* idem, especificidade 0 */
:has(> img)              /* parent selector */
:disabled :checked :required :invalid

::before  ::after        /* precisa content: "" */
::placeholder
::selection
::first-line ::first-letter
```

## Especificidade
```
inline style          1000
#id                    100
.classe / [attr] / :   10
elemento / ::           1
!important            ganha de tudo (evite)
```
Empate é resolvido pela ordem: o último declarado vence.

## Box model
```css
.box {
    box-sizing: border-box;   /* width inclui padding+border */
    width: 200px;
    padding: 1rem;
    border: 1px solid #333;
    margin: 0 auto;           /* centraliza horizontalmente */
}
/* reset comum */
*, *::before, *::after { box-sizing: border-box; }
```

## Unidades
```
px      pixel absoluto
rem     relativo ao root font-size (use para spacing/fonte)
em      relativo ao font-size do elemento
%       relativo ao container
vw vh   1% da largura/altura da viewport
vmin vmax  menor/maior dimensão da viewport
ch      largura do "0" da fonte
fr      fração (grid)
clamp(min, ideal, max)   /* responsivo sem media query */
```

## Cores
```css
color: #ff0000;
color: #f00;
color: rgb(255 0 0 / 0.5);     /* sintaxe moderna com alpha */
color: hsl(0 100% 50% / 0.5);
color: oklch(0.7 0.15 250);    /* perceptualmente uniforme */
color: currentColor;           /* herda a cor do texto */
```

## Flexbox
```css
.container {
    display: flex;
    flex-direction: row;            /* row | column | *-reverse */
    justify-content: space-between; /* eixo principal */
    align-items: center;            /* eixo cruzado */
    flex-wrap: wrap;
    gap: 1rem;
}
.item {
    flex: 1;            /* grow shrink basis em um atalho */
    flex: 0 0 200px;    /* não cresce, não encolhe, base 200px */
    align-self: flex-end;
}
```
`justify-content`: flex-start, center, flex-end, space-between, space-around, space-evenly.

## Grid
```css
.grid {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    grid-template-columns: 200px 1fr 1fr;
    grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); /* responsivo */
    grid-template-rows: auto 1fr auto;
    gap: 1rem;
}
.item {
    grid-column: 1 / 3;        /* da linha 1 até a 3 */
    grid-column: span 2;
    grid-row: 1 / -1;          /* até o fim */
}
/* áreas nomeadas */
.layout {
    display: grid;
    grid-template-areas:
        "header header"
        "sidebar main"
        "footer footer";
}
.header { grid-area: header; }
```

## Position
```css
position: static;    /* padrão */
position: relative;  /* offset relativo a si; cria contexto */
position: absolute;  /* relativo ao ancestral posicionado */
position: fixed;     /* relativo à viewport */
position: sticky;    /* gruda ao rolar */
top: 0; right: 0; bottom: 0; left: 0;
inset: 0;            /* atalho para os quatro */
z-index: 10;
```

## Responsivo
```css
@media (max-width: 768px) { ... }      /* mobile-first usa min-width */
@media (min-width: 769px) and (max-width: 1024px) { ... }
@media (prefers-color-scheme: dark) { ... }
@media (prefers-reduced-motion: reduce) { ... }

/* container queries */
.card-wrap { container-type: inline-size; }
@container (min-width: 400px) { .card { display: flex; } }
```

## Variáveis (custom properties)
```css
:root {
    --cor-primaria: #2563eb;
    --espaco: 1rem;
}
.btn {
    background: var(--cor-primaria);
    padding: var(--espaco) calc(var(--espaco) * 2);
    color: var(--cor-texto, white);   /* fallback */
}
```

## Transitions / animations
```css
.btn {
    transition: background 0.2s ease, transform 0.2s;
}
.btn:hover { transform: scale(1.05); }

@keyframes spin {
    from { transform: rotate(0); }
    to   { transform: rotate(360deg); }
}
.loader {
    animation: spin 1s linear infinite;
}
/* shorthand: name duration timing delay count direction fill */
animation: spin 1s ease-in-out 0s infinite alternate;
```

## Transforms
```css
transform: translate(10px, 20px);
transform: translateX(-50%);     /* centralizar truque */
transform: rotate(45deg);
transform: scale(1.2);
transform: skew(10deg);
transform: translate(-50%, -50%) rotate(45deg);  /* compõe */
transform-origin: center;
```

## Centralização (receitas)
```css
/* flex */
.c { display: flex; justify-content: center; align-items: center; }

/* grid */
.c { display: grid; place-items: center; }

/* absolute */
.c { position: absolute; top: 50%; left: 50%;
     transform: translate(-50%, -50%); }
```

## Texto e tipografia
```css
font-family: system-ui, -apple-system, sans-serif;
font-size: clamp(1rem, 2.5vw, 1.5rem);
line-height: 1.5;
font-weight: 600;
letter-spacing: 0.05em;
text-align: center;
text-transform: uppercase;
text-overflow: ellipsis; overflow: hidden; white-space: nowrap;  /* truncar */
/* truncar multilinha */
display: -webkit-box; -webkit-line-clamp: 3;
-webkit-box-orient: vertical; overflow: hidden;
```

## Útil no dia a dia
```css
overflow: hidden | auto | scroll | clip;
object-fit: cover;          /* img preenche sem distorcer */
aspect-ratio: 16 / 9;
cursor: pointer;
pointer-events: none;       /* ignora cliques */
user-select: none;
visibility: hidden;         /* some mas ocupa espaço */
opacity: 0;
backdrop-filter: blur(8px);
scroll-behavior: smooth;
will-change: transform;     /* dica de performance, use com parcimônia */
```
