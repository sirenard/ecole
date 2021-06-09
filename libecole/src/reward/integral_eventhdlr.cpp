#include "scip/scip.h"
#include "scip/type_event.h"

#include "ecole/reward/integral_eventhdlr.hpp"
#include "ecole/scip/model.hpp"
#include "ecole/utility/chrono.hpp"

namespace ecole::reward {

namespace {

auto time_now(bool wall) -> std::chrono::nanoseconds {
	if (wall) {
		return std::chrono::steady_clock::now().time_since_epoch();
	}
	return utility::cpu_clock::now().time_since_epoch();
}

/* Gets the primal bound of the scip model */
auto get_primal_bound(SCIP* scip) {
	switch (SCIPgetStage(scip)) {
	case SCIP_STAGE_TRANSFORMED:
	case SCIP_STAGE_INITPRESOLVE:
	case SCIP_STAGE_PRESOLVING:
	case SCIP_STAGE_EXITPRESOLVE:
	case SCIP_STAGE_PRESOLVED:
	case SCIP_STAGE_INITSOLVE:
	case SCIP_STAGE_SOLVING:
	case SCIP_STAGE_SOLVED:
		return SCIPgetPrimalbound(scip);
	default:
		return SCIPinfinity(scip);
	}
}

/* Gets the dual bound of the scip model */
auto get_dual_bound(SCIP* scip) {
	switch (SCIPgetStage(scip)) {
	case SCIP_STAGE_TRANSFORMED:
	case SCIP_STAGE_INITPRESOLVE:
	case SCIP_STAGE_PRESOLVING:
	case SCIP_STAGE_EXITPRESOLVE:
	case SCIP_STAGE_PRESOLVED:
	case SCIP_STAGE_INITSOLVE:
	case SCIP_STAGE_SOLVING:
	case SCIP_STAGE_SOLVED:
		return SCIPgetDualbound(scip);
	default:
		return -SCIPinfinity(scip);
	}
}

auto is_lp_event(SCIP_EVENTTYPE event) {

	if ((event == SCIP_EVENTTYPE_LPEVENT) | 
		(event == SCIP_EVENTTYPE_FIRSTLPSOLVED) |
		(event == SCIP_EVENTTYPE_LPSOLVED)) {
		return true;
	}
	return false;
}

auto is_bestsol_event(SCIP_EVENTTYPE event) {
	if (event == SCIP_EVENTTYPE_BESTSOLFOUND) {
		return true;
	}
	return false;
}

}  // namespace

/* Catches primal and dual related events */
SCIP_DECL_EVENTINIT(IntegralEventHandler::scip_init) {
	if (extract_primal) {
		SCIP_CALL(SCIPcatchEvent(scip, SCIP_EVENTTYPE_BESTSOLFOUND, eventhdlr, nullptr, nullptr));
	}
	if (extract_dual) {
		SCIP_CALL(SCIPcatchEvent(scip, SCIP_EVENTTYPE_LPEVENT, eventhdlr, nullptr, nullptr));
	}
	return SCIP_OKAY;
}

/* Drops primal and dual related events */
SCIP_DECL_EVENTEXIT(IntegralEventHandler::scip_exit) {
	if (extract_primal) {
		SCIP_CALL(SCIPdropEvent(scip, SCIP_EVENTTYPE_BESTSOLFOUND, eventhdlr, nullptr, -1));
	}
	if (extract_dual) {
		SCIP_CALL(SCIPdropEvent(scip, SCIP_EVENTTYPE_LPEVENT, eventhdlr, nullptr, -1));
	}
	return SCIP_OKAY;
}

/* Calls extract_metrics() to obtain bounds/times at events */
SCIP_DECL_EVENTEXEC(IntegralEventHandler::scip_exec) {
	extract_metrics(scip_, SCIPeventGetType(event));
	return SCIP_OKAY;
}

/* Gets and adds primal/dual bounds and times to vectors */
void IntegralEventHandler::extract_metrics(SCIP* scip, SCIP_EVENTTYPE event_type) {
	if (extract_primal) {
		if ((is_lp_event(event_type)) | (primal_bounds.size() == 0)) {
			primal_bounds.push_back(get_primal_bound(scip));
		} else {
			primal_bounds.push_back(primal_bounds[primal_bounds.size()-1]);
		}
	}
	if (extract_dual) {
		if ((is_bestsol_event(event_type)) | (dual_bounds.size() == 0)) {
			dual_bounds.push_back(get_dual_bound(scip));
		} else {
			dual_bounds.push_back(dual_bounds[dual_bounds.size()-1]);
		}
	}
	times.push_back(time_now(scip));
}

}  // namespace ecole::reward
