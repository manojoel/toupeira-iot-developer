from setuptools import setup, find_packages

setup(
    name="toupeira-iot",
    version="1.0.0",
    description="SDK oficial da plataforma Toupeira IoT",
    author="Toupeira IoT",
    author_email="suporte@toupeira.io",
    url="https://github.com/manojoel/toupeira-iot-developer",
    packages=find_packages(),
    python_requires=">=3.8",
    install_requires=[
        "requests>=2.28",
        "paho-mqtt>=1.6",
    ],
    extras_require={
        "async": [
            "aiohttp>=3.8",
            "asyncio-mqtt>=0.16",
        ],
    },
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Topic :: System :: Hardware",
        "Topic :: Internet of Things",
    ],
)
