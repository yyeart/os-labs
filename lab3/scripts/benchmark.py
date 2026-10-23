"""Замеры полного времени программы: запуск, чтение матрицы и фильтрация.

Матрицы создаются до замеров во временной папке и затем удаляются.
Скрипт запускается внутри Linux VM, где собрана программа filter.
"""

import argparse
import csv
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile
import time


def generate_matrix(filename, size):
    with open(filename, "w", encoding="utf-8") as file:
        file.write(f"{size} {size}\n")
        for row in range(size):
            values = []
            for col in range(size):
                values.append(str(((row * size + col) % 100) / 10))
            file.write(" ".join(values) + "\n")


def measure(command):
    start = time.perf_counter()
    # check=True останавливает эксперимент, если C-программа вернула ошибку.
    # Обычный вывод скрываем, сообщения об ошибках оставляем в терминале.
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
    return time.perf_counter() - start


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", default="./filter", help="Путь к программе")
    parser.add_argument("--sizes", nargs="+", type=int, default=[512, 1024],
                        help="Стороны квадратных матриц")
    parser.add_argument("--threads", nargs="+", type=int, default=[1, 2, 4],
                        help="Количество потоков; 1 добавляется для сравнения")
    parser.add_argument("--window", type=int, default=3, help="Размер окна")
    parser.add_argument("--iterations", type=int, default=5, help="Число проходов")
    parser.add_argument("--repeats", type=int, default=3, help="Повторов замера")
    parser.add_argument("--output", default="results/benchmark.csv",
                        help="Файл результатов")
    args = parser.parse_args()

    if min(args.sizes + args.threads + [args.iterations, args.repeats]) < 1:
        parser.error("Размеры, потоки, проходы и повторы должны быть положительными")
    if args.window < 1 or args.window % 2 == 0:
        parser.error("Размер окна должен быть положительным и нечётным")

    program = str(Path(args.program).resolve())
    thread_counts = sorted(set([1] + args.threads))
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    columns = ["size", "window", "iterations", "threads_requested", "threads_used"]
    columns += [f"run_{i + 1}_seconds" for i in range(args.repeats)]
    columns += ["median_seconds", "speedup", "efficiency"]

    print("Измеряется полное время программы, включая запуск и чтение файла.")
    print("size       threads    median, s    speedup    efficiency")

    # TemporaryDirectory удаляет созданные матрицы после эксперимента.
    with tempfile.TemporaryDirectory(prefix="lab3-benchmark-") as directory:
        with output.open("w", newline="", encoding="utf-8") as file:
            writer = csv.writer(file)
            writer.writerow(columns)

            for size in args.sizes:
                input_file = Path(directory) / f"matrix_{size}.txt"
                generate_matrix(input_file, size)
                serial_time = None

                for threads in thread_counts:
                    command = [program, "-i", str(input_file),
                               "-w", str(args.window), "-k", str(args.iterations),
                               "-t", str(threads)]

                    measure(command)  # Один прогревочный запуск, не входит в CSV.
                    times = []
                    for _ in range(args.repeats):
                        times.append(measure(command))

                    median_time = statistics.median(times)
                    if threads == 1:
                        serial_time = median_time

                    used_threads = min(threads, size)
                    speedup = serial_time / median_time
                    efficiency = speedup / used_threads

                    result = [size, args.window, args.iterations, threads, used_threads]
                    result += [f"{value:.9f}" for value in times]
                    result += [f"{median_time:.9f}", f"{speedup:.6f}",
                               f"{efficiency:.6f}"]
                    writer.writerow(result)
                    file.flush()

                    print(f"{size:<10} {used_threads:<10} {median_time:<12.6f} "
                          f"{speedup:<10.3f} {efficiency:.3f}", flush=True)

    print(f"Результаты: {output}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"Ошибка: {error}", file=sys.stderr)
        sys.exit(1)
