import numpy as np
import pandas as pd

def coef_ci_table(result):
    params = result.params
    ci = result.conf_int()
    out = pd.DataFrame({
        "coef": params,
        "ci_low": ci[0],
        "ci_high": ci[1],
        "p": result.pvalues
    })
    return out

def test_effect(result, terms):
    print("Testing:", terms)
    print(result.wald_test(terms))