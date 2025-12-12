#!/usr/bin/env python3
"""
Record a short voice question, send it to ElevenLabs STT, answer via a chatbot,
and speak the response with ElevenLabs TTS.

Usage:
  python voice_input_chatbot.py

Dependencies:
  pip install requests sounddevice soundfile playsound
  (optional) set up `tgpt` CLI for LLM answers
"""

import os
import shutil
import subprocess
import sys
import tempfile

import requests
import sounddevice as sd
import soundfile as sf
from playsound import playsound

# Hardcoded API key per request. Replace for your own key management.
ELEVENLABS_API_KEY = ""
VOICE_ID = os.environ.get("ELEVENLABS_VOICE_ID", "21m00Tcm4TlvDq8ikWAM")

SAMPLE_RATE = 16000
RECORD_SECONDS = float(os.environ.get("RECORD_SECONDS", 6))


def ensure_api_key() -> None:
    if not ELEVENLABS_API_KEY:
        sys.exit("Missing ElevenLabs API key. Edit ELEVENLABS_API_KEY in the script.")


def record_audio() -> str:
    """Record audio from default microphone and return path to a temp WAV file."""
    sd.default.samplerate = SAMPLE_RATE
    sd.default.channels = 1
    print(f"🎙️  Speak now (listening for {RECORD_SECONDS} seconds)...")
    audio = sd.rec(int(RECORD_SECONDS * SAMPLE_RATE), dtype="float32")
    sd.wait()

    tmp = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
    sf.write(tmp.name, audio, SAMPLE_RATE)
    tmp.close()
    return tmp.name


def transcribe(path: str) -> str:
    url = "https://api.elevenlabs.io/v1/speech-to-text"
    headers = {"xi-api-key": ELEVENLABS_API_KEY}
    # Use a currently supported STT model.
    data = {"model_id": "scribe_v2"}
    with open(path, "rb") as f:
        files = {"file": f}
        resp = requests.post(url, headers=headers, data=data, files=files, timeout=60)
    _raise_elevenlabs_for_status(resp, "speech-to-text")
    text = resp.json().get("text", "").strip()
    if not text:
        raise ValueError("No transcription returned from ElevenLabs.")
    return text


def text_to_speech(text: str, voice_id: str = VOICE_ID) -> str:
    url = f"https://api.elevenlabs.io/v1/text-to-speech/{voice_id}/stream"
    headers = {
        "xi-api-key": ELEVENLABS_API_KEY,
        "Content-Type": "application/json",
        "Accept": "audio/mpeg",
    }
    payload = {
        "text": text,
        "model_id": "eleven_turbo_v2",
        "voice_settings": {"stability": 0.4, "similarity_boost": 0.8},
    }
    with requests.post(url, headers=headers, json=payload, stream=True, timeout=60) as resp:
        _raise_elevenlabs_for_status(resp, "text-to-speech")
        tmp = tempfile.NamedTemporaryFile(suffix=".mp3", delete=False)
        for chunk in resp.iter_content(chunk_size=1024):
            if chunk:
                tmp.write(chunk)
        tmp.close()
    return tmp.name


def _raise_elevenlabs_for_status(resp: requests.Response, action: str) -> None:
    if resp.status_code == 401:
        sys.exit(
            f"401 Unauthorized during {action}. Check ELEVENLABS_API_KEY hardcoded in this script."
        )
    try:
        resp.raise_for_status()
    except requests.HTTPError:
        detail = resp.text.strip()
        sys.exit(f"ElevenLabs {action} failed ({resp.status_code}): {detail}")


def play_audio(path: str) -> None:
    try:
        playsound(path)
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


def generate_reply(user_text: str) -> str:
    """Chatbot reply using tgpt CLI if available, else echo."""
    tgpt_path = shutil.which("tgpt")
    if tgpt_path:
        try:
            return _tgpt_chat(tgpt_path, user_text)
        except Exception as exc:
            print(f"[warn] tgpt failed ({exc}), falling back to echo.")
    return f"You said: {user_text}"


def _tgpt_chat(tgpt_path: str, user_text: str) -> str:
    """Call tgpt CLI and return its stdout."""
    proc = subprocess.run(
        [tgpt_path, f"Answer briefly and simply: {user_text}"],
        check=True,
        capture_output=True,
        text=True,
    )
    out = proc.stdout.strip()
    if not out:
        raise RuntimeError("tgpt returned no output")
    return out


def main() -> None:
    ensure_api_key()
    print("Voice input chatbot ready. Say 'stop' or 'exit' to quit.")

    while True:
        audio_path = record_audio()
        try:
            user_text = transcribe(audio_path)
        finally:
            try:
                os.remove(audio_path)
            except OSError:
                pass

        print("🧍 You:", user_text)
        if user_text.lower() in {"stop", "exit", "quit"}:
            print("👋 Bye!")
            break

        reply = generate_reply(user_text)
        print("🤖 Bot:", reply)

        audio_reply_path = text_to_speech(reply)
        play_audio(audio_reply_path)


if __name__ == "__main__":
    main()

