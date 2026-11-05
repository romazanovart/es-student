import argparse
import struct
import time
import wave
from datetime import datetime

import serial
from serial.tools import list_ports

VENDOR_ID = 0x2E8A
PRODUCT_ID = 0x000A

PROJECT = "321-audio-recorder"
SAMPLE_RATE = 44100

SYNC_WORD = b"\xa5\x5a"
HEADER = struct.Struct("<HHH")

BIAS_SAMPLES = SAMPLE_RATE // 10

MELODY_BPM = 126
MELODY = [
    ("E5", 0.5), ("F5", 1), ("D5", 0.5), ("E5", 1), ("E5", 0.5), ("F5", 1),
    ("D5", 1), ("E5", 1.5), ("C5", 1), ("B4", 1.5), ("D5", 4.5), (None, 2),
    ("E5", 0.5), ("F5", 1), ("D5", 0.5), ("E5", 1), ("C5", 0.5), ("D5", 1.25),
    ("B4", 0.25), ("B4", 2), ("G#4", 0.5), ("F4", 0.5), ("E4", 2), (None, 2),
    ("E5", 0.5), ("F5", 1), ("D5", 0.5), ("E5", 1), ("E5", 0.5), ("F5", 1),
    ("D5", 1), ("E5", 1.5), ("C5", 1), ("B4", 1.5), ("D5", 4.5), (None, 1.5),
    ("E4", 0.5), ("D5", 0.5), ("C5", 0.5), ("C5", 0.5), ("B4", 0.5),
    ("D5", 0.5), ("C5", 0.5), ("B4", 0.5), ("A4", 0.5), ("A4", 3),
]

NOTE_HZ = {
    "E4": 330, "F4": 349, "G#4": 415, "A4": 440, "B4": 494,
    "C5": 523, "D5": 587, "E5": 659, "F5": 698,
}

NOTE_GAP_S = 0.04

MELODY_MARGIN_S = 0.5


def find_board():
    for port in list_ports.comports():
        if port.vid == VENDOR_ID and port.pid == PRODUCT_ID:
            return port
    return None


def read_line_until(port, prefix, timeout_s):
    """Читает строки ответа, пока не встретит строку с началом prefix."""
    deadline = time.monotonic() + timeout_s
    text = b""
    while time.monotonic() < deadline:
        text += port.read(256)
        while b"\n" in text:
            line, text = text.split(b"\n", 1)
            line = line.decode("ascii", "replace").strip()
            if line.startswith(prefix):
                return line, text
    return None, text


class Receiver:
    """Собирает кадры из потока байтов и считает пропуски по номерам."""

    def __init__(self, bits):
        self.bits = bits
        self.sample_bytes = 2 if bits == 16 else 1
        self.expected = 1024 // self.sample_bytes
        self.buffer = bytearray()
        self.last_number = None
        self.frames = 0
        self.lost = 0
        self.skipped = 0
        self.samples = []

    def feed(self, data):
        self.buffer += data
        while True:
            start = self.buffer.find(SYNC_WORD)
            if start < 0:
                keep = 1 if self.buffer.endswith(SYNC_WORD[:1]) else 0
                self.skipped += len(self.buffer) - keep
                del self.buffer[:len(self.buffer) - keep]
                return
            if start > 0:
                self.skipped += start
                del self.buffer[:start]
            if len(self.buffer) < HEADER.size:
                return
            sync_word, number, count = HEADER.unpack_from(self.buffer)
            if count != self.expected:
                self.skipped += 1
                del self.buffer[:1]
                continue
            size = HEADER.size + count * self.sample_bytes
            if len(self.buffer) < size:
                return
            layout = "<%dH" % count if self.bits == 16 else "<%dB" % count
            self.samples.extend(struct.unpack_from(layout, self.buffer, HEADER.size))
            if self.last_number is not None:
                self.lost += (number - self.last_number - 1) & 0xFFFF
            self.last_number = number
            self.frames += 1
            del self.buffer[:size]


def melody_events():
    """Моменты от начала мелодии, в которые прибору нужно сменить тон."""
    beat_s = 60 / MELODY_BPM
    events = []
    t = 0
    for note, beats in MELODY:
        if note is not None:
            events.append((t, 0))
            events.append((t + NOTE_GAP_S, NOTE_HZ[note]))
        t += beats * beat_s
    events.append((t, 0))
    return events, t


def record(board, seconds, bits, melody):
    exchange = []
    events = []
    if melody:
        events, duration = melody_events()
        events = [(MELODY_MARGIN_S + t, hz) for t, hz in events]
        seconds = duration + 2 * MELODY_MARGIN_S

    with serial.Serial(board.device, timeout=0.05) as port:
        time.sleep(0.2)
        port.reset_input_buffer()

        command = "stream_start" if bits == 16 else "stream_start 8"
        port.write((command + "\r").encode("ascii"))
        exchange.append(("-->", command))
        answer, rest = read_line_until(port, "stream:", 2)
        if answer is None or not answer.startswith("stream: started"):
            raise SystemExit("Прибор не запустил поток: " + str(answer))
        exchange.append(("<--", answer))

        receiver = Receiver(bits)
        receiver.feed(rest)
        started = time.monotonic()
        tones = 0
        while time.monotonic() - started < seconds:
            receiver.feed(port.read(max(1, port.in_waiting)))
            while events and time.monotonic() - started >= events[0][0]:
                hz = events.pop(0)[1]
                port.write(("generate_tone %d\r" % hz).encode("ascii"))
                tones += 1
                if hz == 0 and events:
                    gap_end = time.monotonic() - started + NOTE_GAP_S
                    events[0] = (max(events[0][0], gap_end), events[0][1])
        elapsed = time.monotonic() - started
        if melody:
            exchange.append(("-->", "generate_tone: %d команд, мелодия %.1f с" % (tones, seconds)))

        port.write(b"stream_stop\r")
        exchange.append(("-->", "stream_stop"))

        tail = b""
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline and b"stream: stopped" not in tail:
            data = port.read(4096)
            tail += data
            receiver.feed(data)
        answer, _ = read_line_until_bytes(tail, "stream: stopped")
        exchange.append(("<--", answer or "нет ответа"))
    return receiver, exchange, elapsed


def read_line_until_bytes(data, prefix):
    start = data.find(prefix.encode("ascii"))
    if start < 0:
        return None, data
    end = data.find(b"\n", start)
    line = data[start:end if end >= 0 else len(data)]
    return line.decode("ascii", "replace").strip(), data


def remove_dc(samples):
    """Убирает постоянную составляющую: у мелодии она меняется от ноты к паузе."""
    out = []
    prev_x = samples[0] if samples else 0
    y = 0.0
    for x in samples:
        y = x - prev_x + 0.995 * y
        prev_x = x
        out.append(y)
    return out


def save_wav(path, samples, bits, melody):
    if melody:
        samples = [x / 4 for x in remove_dc(samples)]
        bias = 0.0
    else:
        bias = sum(samples[:BIAS_SAMPLES]) / min(len(samples), BIAS_SAMPLES)
    with wave.open(path, "wb") as wav:
        wav.setnchannels(1)
        wav.setframerate(SAMPLE_RATE)
        if bits == 16:
            wav.setsampwidth(2)
            values = [max(-32768, min(32767, round((s - bias) * 16))) for s in samples]
            wav.writeframes(struct.pack("<%dh" % len(values), *values))
        else:
            wav.setsampwidth(1)
            values = [max(0, min(255, round(s - bias) + 128)) for s in samples]
            wav.writeframes(bytes(values))
    return bias


def write_log(path, board, task, exchange, receiver, elapsed, wav_path, bias):
    samples = receiver.samples
    with open(path, "w", encoding="utf-8") as log:
        log.write("задание: " + task + "\n")
        log.write("проект: " + PROJECT + "\n")
        log.write("устройство: %04x:%04x\n" % (board.vid, board.pid))
        log.write("серийный номер: " + str(board.serial_number) + "\n")
        log.write("порт: " + board.device + "\n")
        log.write("начало: " + datetime.now().isoformat(timespec="seconds") + "\n")
        for direction, text in exchange[:2]:
            log.write("%s %s\n" % (direction, text))
        log.write("принято: кадров %d, отсчётов %d, пропущено кадров %d, байтов вне кадров %d\n"
                  % (receiver.frames, len(samples), receiver.lost, receiver.skipped))
        log.write("скорость: %.0f отсчётов в секунду за %.2f с\n" % (len(samples) / elapsed, elapsed))
        if samples:
            log.write("отсчёты: наименьший %d, наибольший %d, средний %.1f\n"
                      % (min(samples), max(samples), sum(samples) / len(samples)))
        if wav_path:
            log.write("файл: %s, %d бит, %d Гц, %.2f с, смещение %.1f\n"
                      % (wav_path, receiver.bits, SAMPLE_RATE, len(samples) / SAMPLE_RATE, bias))
        for direction, text in exchange[2:]:
            log.write("%s %s\n" % (direction, text))


parser = argparse.ArgumentParser(description="Рекордер звука на Raspberry Pi Pico")
parser.add_argument("--seconds", type=float, default=5, help="длительность записи, с")
parser.add_argument("--wav", help="сохранить запись в файл WAV")
parser.add_argument("--bits", type=int, choices=(8, 16), default=16, help="разрядность потока")
parser.add_argument("--melody", action="store_true",
                    help="сыграть припев тоном с GP20 и записать его, длительность по мелодии")
args = parser.parse_args()
if args.melody and not args.wav:
    parser.error("для мелодии укажите файл: --wav <имя>.wav")

board = find_board()
if board is None:
    raise SystemExit("Плата не найдена. Проверьте кабель и запишите на плату прошивку прибора.")

if args.melody:
    print("Плата на порту %s, играю и пишу припев" % board.device)
else:
    print("Плата на порту %s, пишу %.1f с" % (board.device, args.seconds))
receiver, exchange, elapsed = record(board, args.seconds, args.bits, args.melody)

bias = None
if args.wav:
    bias = save_wav(args.wav, receiver.samples, args.bits, args.melody)
    log_path = args.wav.rsplit(".", 1)[0] + ".log"
    if args.melody:
        task = "3.2.5"
    else:
        task = "3.2.7д" if args.bits == 8 else "3.2.7"
else:
    log_path = "stream.log"
    task = "3.2.4"

write_log(log_path, board, task, exchange, receiver, elapsed, args.wav, bias)
print("Кадров %d, отсчётов %d, пропущено кадров %d" % (receiver.frames, len(receiver.samples), receiver.lost))
if args.wav:
    print("Запись сохранена в " + args.wav)
print("Итог записан в " + log_path)
