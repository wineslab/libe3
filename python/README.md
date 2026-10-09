# libe3py

Python binding of [libe3](https://github.com/wineslab/libe3), the C++ library that implements the O-RAN
E3 interface between a RAN node and its dApps. The [dApp library](https://github.com/wineslab/dApp-library)
(`pip install dapps`) uses it for E3AP; see
[`swig/README.md`](https://github.com/wineslab/libe3/blob/main/swig/README.md) for the API.

libe3py is built when you install it, against the libe3 installed on the machine, and it must be the same
version. Install libe3 once:

```bash
git clone --branch <version> https://github.com/wineslab/libe3 && cd libe3
./build_libe3 -I                         # build dependencies, swig and python3-dev included
./build_libe3 --all-encodings --install  # into /usr/local, asks for sudo
```

then, in any virtual environment and without root:

```bash
pip install libe3py==<version>
python -c "import libe3py; print(libe3py.__version__)"
```
