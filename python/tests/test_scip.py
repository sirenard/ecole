import importlib

import pytest

import ecole.scip


requires_pyscipopt = pytest.mark.skipif(
    importlib.find_loader("pyscipopt") is None, reason="PyScipOpt is not installed.",
)


def test_equality(model):
    assert model == model
    assert model != 33


def test_clone(model):
    model_copy = model.clone()
    assert model is not model_copy
    assert model != model_copy


@requires_pyscipopt
def test_from_pyscipopt_shared():
    """Ecole share same pointer."""
    import pyscipopt.scip

    param, value = "concurrent/paramsetprefix", "ecole_dummy"
    pyscipopt_model = pyscipopt.scip.Model()
    pyscipopt_model.setParam(param, value)
    ecole_model = ecole.scip.Model.from_pyscipopt(pyscipopt_model)
    assert ecole_model.get_param(param) == value


@requires_pyscipopt
def test_from_pyscipopt_ownership():
    """PyScipOpt model remains valid if Ecole model goes out of scope."""
    import pyscipopt.scip

    pyscipopt_model = pyscipopt.scip.Model()
    # ecole_model becomes pointer owner
    ecole_model = ecole.scip.Model.from_pyscipopt(pyscipopt_model)
    assert not pyscipopt_model._freescip
    del ecole_model
    pyscipopt_model.getParams()


@requires_pyscipopt
def test_from_pyscipopt_no_ownership(model):
    """Fail to convert if PyScipOpt does not have ownership."""
    pyscipopt_model = model.as_pyscipopt()
    with pytest.raises(ecole.scip.Exception):
        ecole.scip.Model.from_pyscipopt(pyscipopt_model)


@requires_pyscipopt
def test_as_pyscipopt_shared(model):
    """PyScipOpt share same pointer."""
    param, value = "concurrent/paramsetprefix", "ecole_dummy"
    model.set_param(param, value)
    pyscipopt_model = model.as_pyscipopt()
    assert pyscipopt_model.getParam(param) == value


@requires_pyscipopt
def test_as_pyscipopt_ownership(model):
    """PyScipOpt model remains valid if Ecole model goes out of scope."""
    # Making a copy to be sure no reference is held elsewhere
    ecole_model = model.clone()
    # ecole_model remains pointer owner
    pyscipopt_model = ecole_model.as_pyscipopt()
    assert not pyscipopt_model._freescip
    del ecole_model
    # Try to access some value
    pyscipopt_model.getParams()


def test_exception(model):
    with pytest.raises(ecole.scip.Exception):
        model.get_param("not_a_param")


names_types = (
    ("branching/preferbinary", bool),  # Bool param
    ("conflict/maxlploops", int),  # Int param
    ("limits/nodes", int),  # Long int param
    ("limits/time", float),  # Real param
    ("branching/pscost/strategy", str),  # Char param
    ("concurrent/paramsetprefix", str),  # String param
)


@pytest.mark.parametrize("name,param_type", names_types)
def test_get_param(model, name, param_type):
    assert type(model.get_param(name)) == param_type


@pytest.mark.parametrize("name,param_type", names_types)
def test_set_param(model, name, param_type):
    if param_type is str:
        value = "v"  # A value accepted for the Char parameter
    else:
        value = param_type(1)  # Cast one to the required type
    model.set_param(name, value)
    assert model.get_param(name) == value
