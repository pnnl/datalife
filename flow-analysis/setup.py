from setuptools import setup, find_packages

setup(
    name="datalife-analyze",
    version="0.1",
    author="Hyungro Lee, Lenny Guo, Jesun Firoz, Nathan R. Tallent",
    author_email="<firstname>.<lastname>@pnnl.gov",
    description="DataLife is measurement and analysis toolset for distributed scientific workflows.",
    license="BSD",
    url="https://github.com/pnnl/datalife",
    keywords="distributed workflows, performance analysis, data flow lifecycles, storage bottlenecks, caterpillar tree",
    classifiers=[
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: BSD License",
        "Development Status :: 4 - Beta",
    ],

    package_dir = { "datalife": "src" },
    packages = find_packages(),
    install_requires=[
        "networkx",
        "numpy",
        "pandas",
        "plotly",
        "pyaml",
        "matplotlib"
    ],

    scripts=['src/datalife-analyze.py'] 
)
