#!/usr/bin/env python3
"""
Minimal HTTP server: accept audio (URL or uploaded file), transcribe with ElevenLabs,
reply via tgpt, and return the spoken answer as audio/mpeg.

Endpoints:
  GET  /chat?audio_url=...           -> returns mp3 audio of the reply
  POST /chat (form-data, key=audio)  -> returns mp3 audio of the reply

Run:
  pip install flask requests
  export ELEVENLABS_API_KEY="sk-..."  # or edit the constant below
  python voice_server.py
"""

import os
import shutil
import subprocess
import tempfile
from typing import Optional

import requests
from flask import Flask, request, send_file, jsonify

# Hardcoded API key per earlier request. Replace with your own key management.
ELEVENLABS_API_KEY = os.environ.get("ELEVENLABS_API_KEY", "")
VOICE_ID = os.environ.get("ELEVENLABS_VOICE_ID", "21m00Tcm4TlvDq8ikWAM")

VERBOSE = os.environ.get("VERBOSE", "1") != "0"

app = Flask(__name__)


def log(msg: str) -> None:
    if VERBOSE:
        print(f"[log] {msg}")


def ensure_api_key() -> None:
    if not ELEVENLABS_API_KEY:
        raise RuntimeError("Missing ElevenLabs API key. Set ELEVENLABS_API_KEY or edit the constant.")


@app.route("/chat", methods=["GET", "POST"])
def chat() -> "flask.Response":
    ensure_api_key()
    audio_path = None
    if request.method == "POST":
        uploaded = request.files.get("audio")
        if not uploaded:
            return jsonify({"error": "audio (file) is required in form-data"}), 400
        audio_path = save_uploaded_audio(uploaded)
        log(f"Received uploaded audio -> {audio_path}")
    else:
        audio_url = request.args.get("audio_url")
        if not audio_url:
            return jsonify({"error": "audio_url is required"}), 400
        log(f"Received audio_url={audio_url}")
        audio_path = download_audio(audio_url)

    try:
        user_text = transcribe(audio_path)
        command_reply = maybe_execute_command(user_text)
        reply = command_reply if command_reply else generate_reply(user_text)
        reply_audio_path = text_to_speech(reply)
    except Exception as exc:
        log(f"Error: {exc}")
        return jsonify({"error": str(exc)}), 500
    finally:
        try:
            os.remove(audio_path)
        except Exception:
            pass

    log(f"Returning TTS audio for reply: {reply}")
    return send_file(reply_audio_path, mimetype="audio/mpeg", as_attachment=False)


def download_audio(url: str) -> str:
    """Download MP3 from URL to a temp file."""
    log(f"Downloading audio from {url}")
    resp = requests.get(url, stream=True, timeout=60)
    resp.raise_for_status()
    tmp = tempfile.NamedTemporaryFile(suffix=".mp3", delete=False)
    for chunk in resp.iter_content(chunk_size=8192):
        if chunk:
            tmp.write(chunk)
    tmp.close()
    log(f"Downloaded audio to {tmp.name}")
    return tmp.name


def save_uploaded_audio(uploaded) -> str:
    """Save uploaded file to a temp MP3 path."""
    tmp = tempfile.NamedTemporaryFile(suffix=".mp3", delete=False)
    uploaded.save(tmp.name)
    tmp.close()
    log(f"Saved uploaded audio to {tmp.name}")
    return tmp.name


def transcribe(path: str) -> str:
    url = "https://api.elevenlabs.io/v1/speech-to-text"
    headers = {"xi-api-key": ELEVENLABS_API_KEY}
    data = {"model_id": "scribe_v2"}
    log(f"Sending audio to ElevenLabs STT with model {data['model_id']}")
    with open(path, "rb") as f:
        files = {"file": f}
        resp = requests.post(url, headers=headers, data=data, files=files, timeout=60)
    log(f"STT status {resp.status_code}")
    _raise_elevenlabs_for_status(resp, "speech-to-text")
    text = resp.json().get("text", "").strip()
    if not text:
        raise ValueError("No transcription returned from ElevenLabs.")
    log(f"Transcription: {text}")
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
    log(f"TTS request with model {payload['model_id']} voice {voice_id}")
    with requests.post(url, headers=headers, json=payload, stream=True, timeout=60) as resp:
        log(f"TTS status {resp.status_code}")
        _raise_elevenlabs_for_status(resp, "text-to-speech")
        tmp = tempfile.NamedTemporaryFile(suffix=".mp3", delete=False)
        for chunk in resp.iter_content(chunk_size=1024):
            if chunk:
                tmp.write(chunk)
        tmp.close()
    log(f"TTS audio saved to {tmp.name}")
    return tmp.name


def _raise_elevenlabs_for_status(resp: requests.Response, action: str) -> None:
    if resp.status_code == 401:
        raise RuntimeError(
            f"401 Unauthorized during {action}. Check ELEVENLABS_API_KEY hardcoded/env."
        )
    resp.raise_for_status()


def generate_reply(user_text: str) -> str:
    """Chatbot reply using tgpt CLI if available, else echo."""
    tgpt_path = shutil.which("tgpt")
    if tgpt_path:
        log(f"tgpt found at {tgpt_path}, sending prompt")
        try:
            return _tgpt_chat(tgpt_path, user_text)
        except Exception as exc:
            log(f"tgpt failed ({exc}), falling back to echo")
    else:
        log("tgpt not found on PATH, using echo fallback")
    return f"You said: {user_text}"


def _tgpt_chat(tgpt_path: str, user_text: str) -> str:
    """Call tgpt CLI and return its stdout."""
    proc = subprocess.run(
        [tgpt_path, f"Answer briefly and simply: {user_text}"],
        check=True,
        capture_output=True,
        text=True,
    )
    cleaned = _strip_tgpt_spinner(proc.stdout)
    if not cleaned:
        raise RuntimeError("tgpt returned no output")
    log(f"tgpt reply: {cleaned}")
    return cleaned


def _strip_tgpt_spinner(output: str) -> str:
    """Remove spinner/loading lines from tgpt output."""
    spinner_prefixes = {"⣾", "⣽", "⣻", "⢿", "⡿", "⣷", "⣯", "⣟"}
    cleaned_lines = []
    for line in output.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if stripped[0] in spinner_prefixes:
            continue
        if "Loading" in stripped:
            continue
        cleaned_lines.append(stripped)
    return "\n".join(cleaned_lines).strip()


def maybe_execute_command(user_text: str) -> Optional[str]:
    """Recognize simple commands and hit device endpoints."""
    normalized = user_text.strip().lower().replace(" ", "_")
    if normalized in {"turn_left", "left"}:
        return _execute_command("left", "http://10.1.61.152/left")
    if normalized in {"turn_right", "right"}:
        return _execute_command("right", "http://10.1.61.152/right")
    if normalized in {"rotate", "rotate_camera", "rotate_device"}:
        return _execute_command("rotate", "http://10.1.61.152/sweep")
    if normalized in {"sweep"}:
        return _execute_command("sweep", "http://10.1.61.152/sweep")
    if normalized in {"stop", "halt"}:
        return _execute_command("stop", "http://10.1.61.152/stop")
    return None


def _execute_command(name: str, url: str) -> str:
    log(f"Executing command {name} via {url}")
    try:
        resp = requests.get(url, timeout=5)
        resp.raise_for_status()
        log(f"Command {name} success: {resp.status_code}")
        return f"{name.replace('_', ' ').title()}."
    except Exception as exc:
        log(f"Command {name} failed: {exc}")
        return f"Failed to {name.replace('_', ' ')}."


if __name__ == "__main__":
    log("Starting server on http://0.0.0.0:5000")
    app.run(host="0.0.0.0", port=5000, debug=False)

