FROM python:3.13-slim-bookworm

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates build-essential libfdt1 libfreetype6 libsdl1.2debian nodejs npm libglib2.0-0 libsdl2-2.0-0 libpixman-1-0 libsndio7.0 \
    && rm -rf /var/lib/apt/lists/*

ENV HOME=/opt/pebble

RUN mkdir -p "$HOME" \
    && pip install --no-cache-dir pebble-tool==5.0.40 \
    && pebble sdk install 4.33.1 \
    && chmod -R a+rwX "$HOME"

WORKDIR /workspace
ENTRYPOINT ["pebble"]
