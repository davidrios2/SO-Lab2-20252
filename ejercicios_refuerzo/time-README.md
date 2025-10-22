# Ejercicio 7 — time.c

Este programa mide el tiempo transcurrido al ejecutar un comando desde la línea de comandos. Se usa como:

```
./time <comando> [argumentos...]
```

Al finalizar, imprime:

```
Elapsed time: <segundos>
```

## Decisiones de diseño

- Medición con `gettimeofday()`:
  - Tomo el tiempo inicial justo antes de `fork()` y el final justo después de `waitpid()`. Esto mide el tiempo de pared (wall-clock) total desde que se inicia el proceso hijo hasta que termina.
  - Se eligió `gettimeofday()` por su disponibilidad sin enlazado adicional; alternativa: `clock_gettime(CLOCK_MONOTONIC)`.
- Ejecución del comando con `execvp()`:
  - Permite usar la `PATH` del entorno y pasar argumentos tal cual: `execvp(argv[1], &argv[1])`.
- Propagación del código de salida:
  - El programa `time` devuelve el mismo código de salida que el comando ejecutado (si finaliza normalmente). Si termina por señal, retorna `128 + número_de_señal`. Si `execvp()` falla, el hijo retorna `127` y el padre retorna ese mismo código.
- Impresión del tiempo siempre:
  - Se imprime el tiempo incluso si el comando falla; esto ayuda a depurar y mantener un comportamiento estable.
- No se usa `system()`: se sigue el enfoque recomendado con `fork()` + `execvp()` + `waitpid()`.

## Flujo de ejecución

1. Validar argumentos: se requiere al menos un comando (`argc >= 2`).
2. Registrar tiempo inicial con `gettimeofday()`.
3. `fork()` para crear el hijo.
   - Hijo: invoca `execvp()` con el comando y sus argumentos; si falla, imprime error y termina con `127`.
   - Padre: espera con `waitpid()` hasta que el hijo termine.
4. Registrar tiempo final.
5. Calcular diferencia en segundos y mostrar `Elapsed time: <segundos>` con 5 decimales.
6. Retornar el código de salida del hijo.

## Compilación y ejecución

- Compilación (Linux/WSL):
  - `gcc -O2 -Wall -o time ejercicios_refuerzo/time.c`
- Ejemplos de uso:
  - `./time ls`
  - `./time ls -l /bin`
  - `./time sleep 1`
  - `./time echo "Hola mundo"`

## Pruebas manuales

- Comando simple:
  - `./time ls`
  - Verificar que imprime `Elapsed time: <valor>` y retorna `0`.
- Con argumentos:
  - `./time ls -l /bin`
  - Debe listar `/bin` y el tiempo ser > 0.
- Tiempo conocido (`sleep`):
  - `./time sleep 1` → tiempo ~ 1.00 s (tolerancia ±0.02 s).
  - `./time sleep 0.1` → tiempo ~ 0.10 s (si `sleep` soporta fracciones).
- Comando que falla:
  - `./time /bin/false` → imprime tiempo y retorna `1`.
  - `./time no-such-command` → stderr indica error de `execvp` y retorna `127`.
- Salida y redirección:
  - `./time echo hello > /tmp/out` → `Elapsed time: ...` sigue en stdout del proceso `time`.
- Señales:
  - `./time sh -c 'sleep 5'` y desde otra terminal enviar `SIGINT` al proceso hijo. El retorno debe ser `130` (128 + 2) si se terminó por `SIGINT`.

## Edge cases

- Sin comando:
  - `./time` → muestra `Usage: time <command> [args...]` en stderr y retorna `1`.
- Argumentos con espacios/quotes:
  - `./time echo "hola mundo"` → los quotes los maneja el shell, `time` recibe un solo argumento "hola mundo".
- Rutas absolutas y relativas:
  - `./time /bin/ls` o `./time ./script.sh` (si es ejecutable).
- Comando muy rápido:
  - Resultados cercanos a 0.0; la precisión de microsegundos de `gettimeofday()` es suficiente para reportar con 5 decimales.
- Efecto de `fork()`:
  - La medición incluye el coste de `fork()` y la espera; es despreciable comparado con la ejecución típica, y se documenta aquí como decisión consciente.

## Posibles mejoras (no implementadas aquí)

- Usar `clock_gettime(CLOCK_MONOTONIC)` para medir tiempo monotónico (más robusto ante cambios de reloj del sistema).
- Medición con inicio en el hijo (previo a `exec`):
  - Pasar el timestamp del hijo al padre vía `pipe()` para excluir el overhead de `fork()`. Aumenta complejidad, poca ganancia práctica.
- Formatos adicionales:
  - Mostrar milisegundos explícitos, o desglose en `mm:ss.mmm`.