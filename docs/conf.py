# Configuration file for the Sphinx documentation builder.

import os
import sys

# Add the Python package to sys.path for autodoc
sys.path.insert(0, os.path.abspath(os.path.join('..', 'python', 'src')))

# -- Project information -----------------------------------------------------

project = 'libfitsverify'
copyright = '2026, Demitri Muna'
author = 'Demitri Muna'
release = '1.0.0'

# -- General configuration ---------------------------------------------------

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'sphinx.ext.viewcode',
    'sphinx.ext.napoleon',
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# -- Options for HTML output -------------------------------------------------

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

html_theme_options = {
    'navigation_depth': 3,
    'collapse_navigation': False,
}

# -- Autodoc configuration ---------------------------------------------------

autodoc_member_order = 'bysource'
autodoc_default_options = {
    'members': True,
    'undoc-members': False,
    'show-inheritance': True,
}

# -- Napoleon configuration (NumPy-style docstrings) -------------------------

napoleon_google_docstrings = False
napoleon_numpy_docstrings = True
napoleon_include_init_with_doc = True
napoleon_use_param = True
napoleon_use_rtype = True

# -- Intersphinx configuration -----------------------------------------------

intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
    'astropy': ('https://docs.astropy.org/en/stable/', None),
}
