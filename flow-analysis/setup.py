from setuptools import setup

setup(
    name="datalife-analyze",
    version="0.1",
    author="Hyungro Lee, Lenny Guo, Jesun Firoz, Nathan R. Tallent",
    author_email="<firstname>.<lastname>@pnnl.gov",
    description="DataLife is measurement and analysis toolset for distributed scientific workflows.",
    license="BSD",
    url="https://github.com/pnnl/datalife",
    classifiers=[
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: BSD License",
        "Development Status :: 4 - Beta",
    ],

    packages=['community'],
    install_requires=[
        "networkx",
        "numpy"
    ],

    scripts=['bin/community'] # TODO: from https://github.com/taynaud/python-louvain
)
