# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
#
# Builds the Word editions with page numbers in the table of contents, the same
# on every host. From the root of the repository:
#
#   docker build -t libe3-oran-docx tools/oran_docx
#   docker run --rm -u "$(id -u):$(id -g)" -v "$PWD":/src libe3-oran-docx docs/e3ap
#
# The Liberation fonts have the metrics of Arial, Times New Roman and Courier New,
# so the LibreOffice layout breaks pages where Word does, or within a line or two.
FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        python3 libreoffice-writer-nogui poppler-utils fonts-liberation \
    && rm -rf /var/lib/apt/lists/*

ENV HOME=/tmp
WORKDIR /src
ENTRYPOINT ["python3", "/src/tools/oran_docx/build_docx.py"]
