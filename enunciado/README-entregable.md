# Entregable: Diseño y Testing de "wish" (Test 1)

Este documento resume las decisiones de diseño tomadas, el flujo de pruebas realizado y las instrucciones de uso para el shell simple "wish" implementado según el enunciado. El objetivo de esta entrega fue hacer una versión mínima funcional que pase el Test 1.

## Objetivo
- Implementar una versión básica de `wish` que:
  - Soporte modo interactivo y modo batch.
  - Implemente los built-ins mínimos: `exit` y `cd` (con validación de argumentos).
  - Emita un único mensaje de error estándar: `"An error has occurred\n"`.
  - Sea robusta frente a archivos de entrada con terminaciones de línea Windows (`CRLF`).
  - Pase el Test 1 del framework de `tester/`.

## Decisiones de Diseño
- Entrada y modos
  - `argc == 1` → modo interactivo leyendo de `stdin` y mostrando `wish> ` como prompt.
  - `argc == 2` → modo batch leyendo comandos desde archivo con `fopen(argv[1], "r")`.
  - `argc > 2` o fallo al abrir archivo → imprimir error y salir con `exit(1)`.

- Lectura de líneas
  - Uso de `getline()` para leer líneas arbitrarias.
  - Normalización de terminaciones: se eliminan `\n` y, si queda, también `\r` al final de la línea. Esto evita que tokens contengan `\r` y se comporten como comandos desconocidos.

- Tokenización
  - Tokenización simple por espacios y tabuladores usando `strtok_r()`.
  - Líneas vacías o solo espacios se ignoran sin error.

- Built-ins mínimos
  - `exit`: exige 0 argumentos; si hay argumentos → error. Si correcto, `exit(0)`.
  - `cd`: exige exactamente 1 argumento (el directorio destino). Si 0 o más de 1 → error. Si `chdir()` falla → error.
  - `path`: por ahora no implementado (placeholder). No impacta en el Test 1.

- Manejo de errores (mensaje único)
  - Se implementa `print_error()` que llama a `write(STDERR_FILENO, error_message, strlen(error_message))` donde `error_message` es `"An error has occurred\n"`.
  - El shell imprime este mensaje ante condiciones de error del propio shell (p. ej., argumentos incorrectos, uso incorrecto de built-ins, comandos inexistentes).

- Robustez frente a `CRLF`
  - Al detectar `\n`, se elimina y se decrementa el conteo; luego se revisa si queda `\r` al final y también se elimina.
  - Esta decisión evita duplicados de errores y mismatches por `\r` incrustados en tokens.

## Compilación
- Comando utilizado:
  - `gcc -Wall -O2 -o wish wish.c`
- Nota: el compilador advierte que se ignora el valor de retorno de `write` en `print_error()`; se acepta en esta versión mínima.

## Testing
- Framework usado: `tester/run-tests.sh`.
- Problema detectado: varios archivos de `enunciado/tests` tenían terminaciones `CRLF` (Windows). Eso causaba:
  - Diferencias en `diff` para `.rc` (`0\r\n` vs `0\n`).
  - Tokens con `\r` (p. ej. `"cd\r"`) que se interpretan como comandos desconocidos, generando errores extra.

- Estrategia de normalización:
  - Se duplicó la carpeta original `tests` a `tests-lf` para no modificar los originales:
    - `cp -r tests tests-lf`
  - Conversión CRLF→LF en todos los archivos de `tests-lf`:
    - `find tests-lf -type f -print0 | xargs -0 sed -i 's/\r$//'`
  - Verificación de archivos clave con `hexdump -C`.

- Ejecución de Test 1:
  - Comando:
    - `../tester/run-tests.sh -t 1 -v -d tests-lf`
  - Resultado: `test 1: passed`

- Pruebas manuales de control:
  - `printf "cd\nexit\n" | ./wish` → un solo error, salida limpia.
  - `./wish tests/1.in` sobre `tests-lf` → un solo error, `rc=0`.

## Instrucciones de uso
- Compilar:
  - `gcc -Wall -O2 -o wish wish.c`
- Ejecutar en modo interactivo:
  - `./wish`
- Ejecutar en modo batch:
  - `./wish archivo.txt`
- Ejecutar Test 1 con tests normalizados:
  - `../tester/run-tests.sh -t 1 -v -d tests-lf`

## Próximos pasos (según README-plan.md)
- Implementar `path` (sobrescribe lista de directorios; inicial `/bin`).
- Ejecutar comandos externos usando `access()` y `execv()` con `fork()`, `wait()`.
- Redirección `>` (validaciones estrictas y duplicación de `stdout` y `stderr`).
- Comandos paralelos `&` con espera final de hijos.
- Consolidar manejo de errores, inputs vacíos, trimming y validaciones.

## Fragmentos técnicos clave
- Remoción de `\n` y `\r` (en `wish.c`):
```
// Remove trailing newline and carriage return
if (nread > 0 && line[nread - 1] == '\n') {
    line[nread - 1] = '\0';
    nread--;
}
if (nread > 0 && line[nread - 1] == '\r') {
    line[nread - 1] = '\0';
}
```

- Validación de `cd` (exactamente 1 argumento):
```
else if (strcmp(tokens[0], "cd") == 0) {
    if (ntok != 2) {
        print_error();
        continue;
    }
    if (chdir(tokens[1]) != 0) {
        print_error();
        continue;
    }
}
```

- Validación de `exit` (0 argumentos):
```
if (strcmp(tokens[0], "exit") == 0) {
    if (ntok != 1) {
        print_error();
        continue;
    }
    free(line);
    exit(0);
}
```
 
### Funcionalidad implementada
- PATH por defecto: `['/bin', '/usr/bin']`. El built-in `path` sobrescribe completamente la lista.
- Ejecución de externos: búsqueda secuencial en `path` con `access(…, X_OK)`; `fork` + `execv`; el padre hace `wait`.
- `argv[0]` se mantiene como el nombre del comando (no la ruta absoluta) para reproducir las salidas de utilidades estándar.
- Redirección `>`:
  - Forma válida: `cmd arg1 … > archivo`. Solo un `>` y exactamente un archivo, al final.
  - Redirige `stdout` y `stderr` al mismo archivo (`dup2` a `1` y `2`). Crea/trunca con `O_CREAT|O_WRONLY|O_TRUNC` y permisos `0666` (sujeto a `umask`).
  - Cualquier otra forma (múltiples `>`, sin archivo, `>` en posición incorrecta, falta de comando) → imprime el mensaje de error del shell.
- Paralelización con `&`:
  - Separa la línea en segmentos por `&`. Segmentos vacíos se ignoran.
  - Built-ins (`cd`, `path`, `exit`) se evalúan por segmento. Externos se `fork`ean.
  - Se recogen PIDs y se hace `wait` de todos los hijos al final de la línea. Compatible con redirección por segmento.
- Robustez CRLF:
  - La entrada recorta `\n` y `\r`. Además, se normalizaron los scripts `tests/*.sh` a `LF` para evitar errores de formato al ejecutar shebangs.
 
### Resultados de pruebas
- Compilación: `gcc -Wall -O2 -o wish wish.c`
- Ejecución de la batería completa con entradas normalizadas: `../tester/run-tests.sh -v -d tests-lf`
- Estado: todas las pruebas `1–22` pasan.
 
### Ejemplos de uso
- PATH y externos:
  - `path /bin /usr/bin`
  - `ls -l`
- Redirección:
  - `echo hola > out.txt`
  - `ls /nope > err.txt` (el mensaje de `ls` va a `err.txt` porque `stderr` también redirige)
- Paralelo:
  - `sleep 1 & echo listo & date`
 
### Notas técnicas
- El warning del compilador sobre `write` en `print_error()` es inocuo; se puede silenciar asignando el retorno a `ssize_t` o haciendo un cast a `(void)`.
- La búsqueda de ejecutables usa `access(X_OK)` para evitar lanzar `exec` con rutas inexistentes.
- `argv[0]` conserva el nombre original del comando para que mensajes tipo `ls: …` coincidan con los esperados.
 
### Observaciones sobre los tests
- El directorio `tests` original contenía archivos con `CRLF`, lo cual provocaba falsos negativos en diffs y problemas al ejecutar scripts. Para pruebas consistentes, se usa `tests-lf` (normalizado a `LF`).
- Los scripts `tests/*.sh` fueron normalizados a `LF` para que el kernel no rechace el shebang por `\r`.

---

## Mapa: problema → fragmento de código (y por qué lo resuelve)

- Doble mensaje de error y comandos con `\r` al final (CRLF)
```
// Remoción de fin de línea y \r
if (nread > 0 && line[nread - 1] == '\n') { line[nread - 1] = '\0'; nread--; }
if (nread > 0 && line[nread - 1] == '\r') { line[nread - 1] = '\0'; }
```
Soluciona: evita que tokens terminen en `\r` (p. ej., `"cd\r"`), lo que causaba comandos desconocidos y errores extra.

- `ls:` vs `/bin/ls:` en stderr (coincidir salida esperada)
```
// argv[0] debe ser el nombre del comando
char *args[128]; int argcnt = 0;
args[argcnt++] = tokens[0];
...
execv(candidate, args);
```
Soluciona: mantiene `argv[0] = "ls"`, por lo que utilidades imprimen `ls: ...` en lugar de la ruta completa.

- PATH por defecto y built-in `path`
```
// Al inicio
strncpy(path_list[0], "/bin", MAX_PATH_LEN - 1);
strncpy(path_list[1], "/usr/bin", MAX_PATH_LEN - 1);
path_count = 2;

// Built-in path
path_count = 0;
for (int i = 1; i < ntok && i <= MAX_PATHS; i++) {
    strncpy(path_list[path_count], tokens[i], MAX_PATH_LEN - 1);
    path_list[path_count][MAX_PATH_LEN - 1] = '\0';
    path_count++;
}
```
Soluciona: garantiza ejecutables disponibles por defecto y permite sobrescribir rutas con `path`.

- Redirección `>`: validación estricta y duplicación de salida/errores
```
// Parseo y validaciones
int redir_idx = -1;
for (int i = 0; i < ntok; i++) {
    if (strcmp(tokens[i], ">") == 0) {
        if (redir_idx != -1) { redir_idx = -2; break; }
        redir_idx = i;
    }
}
if (redir_idx >= 0) {
    if (redir_idx != ntok - 2) { print_error(); continue; }
    outfile = tokens[ntok - 1];
    args_end = redir_idx;
}

// En el hijo
int fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
if (fd < 0) { print_error(); _exit(1); }
if (dup2(fd, STDOUT_FILENO) < 0) { print_error(); _exit(1); }
if (dup2(fd, STDERR_FILENO) < 0) { print_error(); _exit(1); }
```
Soluciona: asegura la forma exacta `cmd ... > archivo` y redirige `stdout` y `stderr` al mismo destino.

- Paralelismo con `&`: segmentación, ejecución y espera
```
if (strchr(p, '&') != NULL) {
    pid_t pids[128]; int pcnt = 0;
    for (char *segment = strtok_r(p, "&", &seg_save); segment != NULL; ... ) {
        // tokenizar y lanzar cada segmento (built-ins o externos)
        pid_t pid = fork();
        if (pid == 0) { execv(candidate, args); ... }
        else { pids[pcnt++] = pid; }
    }
    for (int i = 0; i < pcnt; i++) { waitpid(pids[i], &status, 0); }
}
```
Soluciona: permite ejecutar varios comandos en paralelo y sincronizar al final de la línea.

- Built-ins con validación estricta
```
// exit
if (strcmp(tokens[0], "exit") == 0) {
    if (ntok != 1) { print_error(); continue; }
    free(line); exit(0);
}

// cd
else if (strcmp(tokens[0], "cd") == 0) {
    if (ntok != 2) { print_error(); continue; }
    if (chdir(tokens[1]) != 0) { print_error(); continue; }
}
```
Soluciona: cumplimiento del enunciado; evita estados inconsistentes o argumentos extra.

- Mensaje de error único y consistente
```
static const char error_message[] = "An error has occurred\n";
static void print_error(void) {
    write(STDERR_FILENO, error_message, strlen(error_message));
}
```
Soluciona: centraliza el mensaje para todos los errores de shell, fácil de mantener y consistente.

## Lecciones aprendidas
- Normalización de CRLF: los `\r` residuales en tokens provocan comandos “desconocidos” y errores extra. Recortar `\n` y `\r` y normalizar los scripts de pruebas evita falsos negativos.
- `argv[0]` importa: mantener el nombre del comando (p. ej., `ls`) garantiza que los mensajes de error coincidan con los esperados por los tests.
- PATH por defecto y `path`: inicializar `['/bin','/usr/bin']` hace que los casos base funcionen. Un `path` vacío implica que sólo se ejecutan built-ins.
- Validación estricta de `>`: exigir exactamente un archivo al final y redirigir `stdout` y `stderr` al mismo destino simplifica y alinea con el enunciado.
- Paralelismo con `&`: segmentar, ejecutar cada segmento, y sincronizar al final garantiza determinismo en las pruebas. Segmentar también evita que errores en un segmento bloqueen el resto.
- Mensaje de error único: centralizar `An error has occurred\n` evita inconsistencias y facilita la comparación con los outputs esperados.
- Diagnóstico incremental: reproducir el test, inspeccionar diffs con `hexdump -C` y ajustar el código en puntos mínimos (p. ej., `argv[0]`, PATH) acelera llegar al “todo verde”.

## Glosario
- `PATH`: lista de directorios donde se buscan ejecutables al invocar comandos externos.
- `argv[0]`: primer argumento pasado a un proceso; por convención, el nombre del programa que se ejecuta.
- `fork`: crea un proceso hijo que es una copia del proceso padre.
- `execv`: reemplaza la imagen de proceso actual por la de un ejecutable; no vuelve si tiene éxito.
- `waitpid`: espera a que un proceso hijo termine, recopilando su estado de salida.
- `dup2`: duplica un descriptor de archivo a un descriptor específico (p. ej., `STDOUT_FILENO` y `STDERR_FILENO`).
- `access(X_OK)`: verifica permisos de ejecución en una ruta antes de intentar `exec`.
- `umask`: máscara de permisos que limita los permisos efectivos al crear archivos.
- `CRLF/LF`: terminaciones de línea Windows (`\r\n`) vs. Unix (`\n`).
- `shebang`: línea inicial `#!` en scripts que indica el intérprete.
- `stdout/stderr`: flujos estándar de salida y de error, respectivamente.

## Casos borde: redirección `>` y paralelismo `&`

| Caso | Ejemplo de entrada | Comportamiento esperado | Notas |
|---|---|---|---|
| Múltiples `>` | `ls > a > b` | Error del shell, no se ejecuta el comando | Se detecta más de un `>` y se imprime `An error has occurred` |
| `>` sin archivo | `ls >` | Error del shell | Falta el archivo; validación de sintaxis falla |
| Archivo no al final | `ls > out extra` | Error del shell | El `>` debe ir seguido de un único archivo al final |
| Sin comando | `> out` | Error del shell | No hay comando antes del `>` |
| Ruta inválida | `ls > /no/perm/out` | Mensaje de error (redir falla), comando no se ejecuta | La redirección falla al abrir; el hijo imprime error y termina |
| Redirección y argumentos | `echo hola mundo > out.txt` | `out.txt` contiene `hola mundo\n` | `stdout` redirigido; `stderr` también al mismo archivo |
| Paralelo básico | `sleep 1 & echo listo & date` | Ejecuta los tres; espera al final | Segmentos vacíos se ignoran |
| Mix built-in + externo | `cd /tmp & ls > out.txt` | `cd` aplica; `ls` redirige y ambos finalizan | Built-in se evalúa en el padre; el externo se lanza en hijo |
| `path` vacío y externo | `path` luego `ls` | Error del shell (no se encuentra `ls`) | Con `path` vacío, no hay búsqueda de ejecutables |
| `&` contiguos | `cmd1 && cmd2` | Ejecuta `cmd1` y `cmd2`; ignora segmento vacío | Segmentación por `&` descarta tokens vacíos |

Notas adicionales:
- La redirección se aplica en procesos hijo (comandos externos). Los built-ins se evalúan en el proceso padre; en las pruebas no requieren redirección de salida.
- Cada segmento paralelo puede incluir su propia redirección válida; se valida por segmento.
- Errores de sintaxis en redirección o path no detienen la ejecución de otros segmentos válidos en la misma línea.