# TypeScript Cheatsheet

Pressupõe o `javascript.md`. Foca no que TS adiciona sobre JS.

## Tipos básicos
```typescript
let n: number = 42;
let s: string = "hi";
let b: boolean = true;
let arr: number[] = [1, 2, 3];
let arr2: Array<number> = [1, 2];
let tup: [string, number] = ["a", 1];   // tupla
let any1: any;          // desativa checagem (evite)
let unk: unknown;       // any seguro (exige narrowing)
let nada: void;         // função sem retorno
let never1: never;      // nunca retorna (throw/loop infinito)
let n2: number | null;
```

## Inferência
```typescript
let x = 42;             // inferido como number
const y = "hi";         // inferido como literal "hi"
// declare tipo só quando a inferência não basta
```

## Interfaces vs types
```typescript
interface User {
    id: number;
    name: string;
    email?: string;          // opcional
    readonly createdAt: Date; // só leitura
}

type Point = { x: number; y: number };

// interface estende; type usa interseção
interface Admin extends User { role: string; }
type Admin2 = User & { role: string };

// type faz coisas que interface não faz:
type ID = string | number;          // union
type Status = "active" | "inactive"; // literais
```
Regra prática: `interface` para formato de objetos e APIs públicas; `type` para unions, tuplas e composições.

## Unions e narrowing
```typescript
function fmt(x: string | number): string {
    if (typeof x === "string") return x.toUpperCase();
    return x.toFixed(2);     // TS sabe que aqui é number
}

// discriminated union
type Shape =
    | { kind: "circle"; r: number }
    | { kind: "square"; side: number };

function area(s: Shape): number {
    switch (s.kind) {
        case "circle": return Math.PI * s.r ** 2;
        case "square": return s.side ** 2;
    }
}
```

## Funções
```typescript
function add(a: number, b: number): number { return a + b; }
const mul = (a: number, b: number): number => a * b;

function greet(name: string, greeting = "Olá"): string { ... }
function sum(...nums: number[]): number { ... }
function find(id: number): User | undefined { ... }

// sobrecarga
function len(x: string): number;
function len(x: any[]): number;
function len(x: string | any[]): number { return x.length; }
```

## Generics
```typescript
function first<T>(arr: T[]): T | undefined { return arr[0]; }
first([1, 2, 3]);        // T = number
first(["a", "b"]);       // T = string

interface Box<T> { value: T; }

function pair<A, B>(a: A, b: B): [A, B] { return [a, b]; }

// constraint
function longest<T extends { length: number }>(a: T, b: T): T {
    return a.length >= b.length ? a : b;
}
```

## Utility types
```typescript
Partial<User>            // todos os campos opcionais
Required<User>           // todos obrigatórios
Readonly<User>           // todos só leitura
Pick<User, "id" | "name">    // só esses campos
Omit<User, "email">          // tudo menos email
Record<string, number>       // { [k: string]: number }
Exclude<"a"|"b"|"c", "a">    // "b" | "c"
Extract<"a"|"b", "a"|"x">    // "a"
NonNullable<string | null>   // string
ReturnType<typeof fn>        // tipo do retorno
Parameters<typeof fn>        // tupla dos argumentos
Awaited<Promise<string>>     // string
```

## Tipos derivados
```typescript
type Keys = keyof User;              // "id" | "name" | "email"
type NameType = User["name"];        // string (indexed access)
const cfg = { port: 8080 } as const; // literais readonly
type Port = typeof cfg.port;         // 8080

// mapped types
type Optional<T> = { [K in keyof T]?: T[K] };
type Nullable<T> = { [K in keyof T]: T[K] | null };
```

## Enums
```typescript
enum Color { Red, Green, Blue }       // 0, 1, 2
enum Status { Active = "active", Done = "done" }
Color.Red;

// alternativa preferida (union de literais, mais leve)
const Color2 = { Red: "red", Blue: "blue" } as const;
type Color2 = typeof Color2[keyof typeof Color2];
```

## Classes
```typescript
class Point {
    private secret = 42;
    protected base = 0;
    readonly id: number;
    static count = 0;

    constructor(public x: number, public y: number) {  // shorthand
        this.id = ++Point.count;
    }
    get norm(): number { return Math.hypot(this.x, this.y); }
}

interface Drawable { draw(): void; }
class Circle implements Drawable {
    draw(): void { ... }
}

abstract class Animal {
    abstract sound(): string;
}
```

## Type guards e asserções
```typescript
// type predicate
function isString(x: unknown): x is string {
    return typeof x === "string";
}

// asserção (confie no dev, sem checagem em runtime)
const el = document.querySelector(".x") as HTMLInputElement;
const el2 = <HTMLInputElement>document.querySelector(".x");

// non-null assertion
const val = map.get("k")!;     // diz "não é null/undefined"

// satisfies (valida sem alargar o tipo)
const cfg = { port: 8080 } satisfies Record<string, number>;
```

## Tipos para async
```typescript
async function fetchUser(id: number): Promise<User> {
    const r = await fetch(`/api/${id}`);
    return r.json() as Promise<User>;
}
```

## tsconfig essencial
```jsonc
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "strict": true,              // ligue sempre
    "noUncheckedIndexedAccess": true,
    "noImplicitAny": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "outDir": "./dist"
  },
  "include": ["src"]
}
```

## Comandos
```bash
npx tsc                  # compila
npx tsc --watch          # modo watch
npx tsc --noEmit         # só checa tipos, não gera JS
npx tsx script.ts        # roda direto (sem compilar)
```
