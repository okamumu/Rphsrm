#' NHPP-based software reliability model
#'
#' Estimate model parameters for NHPP-based software reliability models.
#'
#' The control argument is a list that can supply any of the following components:
#' \describe{
#'   \item{maxiter}{An integer for the maximum number of iterations in the fitting algorithm.}
#'   \item{reltol}{A numeric value. The algorithm stops if the relative error is
#' less than \emph{reltol} and the absolute error is less than \emph{abstol}.}
#'   \item{abstol}{A numeric value. The algorithm stops if the relative error is
#' less than \emph{reltol} and the absolute error is less than \emph{abstol}.}
#'   \item{trace}{A logical. If TRUE, the intermediate parameters are printed.}
#'   \item{printsteps}{An integer for print.}
#' }
#'
#' @param time A numeric vector for time intervals.
#' @param fault An integer vector for the number of faults detected in time intervals.
#' The fault detected just at the end of time interal is not counted.
#' @param type Either 0 or 1. If 1, a fault is detected just at the end of corresponding time interval.
#' This is used to represent the fault time data. If 0, no fault is detected at the end of interval.
#' @param te A numeric value for the time interval from the last fault to the observation time.
#' @param data A dataframe. The arguments; time, fault, type, te can also be selected as the columns of dataframe.
#' @param srm.names A character vector, indicating the model (\code{\link{srm.models}}).
#' @param selection A character string, indicating the model selection criterion. The default is "AIC".
#' If this is NULL, the method returns the results for all model candidates.
#' @param control A list of control parameters. See Details.
#' @param ... Other parameters.
#' @return A list with components;
#' \item{initial}{A vector for initial parameters.}
#' \item{srm}{A class of NHPP. The SRM with the estiamted parameters.}
#' \item{llf}{A numeric value for the maximum log-likelihood function.}
#' \item{df}{An integer for degrees of freedom.}
#' \item{convergence}{A boolean meaning the alorigthm is converged or not.}
#' \item{iter}{An integer for the number of iterations.}
#' \item{aerror}{A numeric value for absolute error.}
#' \item{rerror}{A numeric value for relative error.}
#' \item{ctime}{A numeric value for computation time.}
#' \item{call}{The method call.}
#' @examples
#' data(dacs)
#' fit.srm.cph(time=sys1[sys1>=0], te=-sys1[sys1<0], phase=10)
#' fit.srm.cph(fault=tohma, phase=10)
#' @export

fit.srm.cph <- function(time = NULL, fault = NULL, type = NULL, te = NULL, data = data.frame(),
  phase = 2:10, selection = "AIC", control = list(), ...) {
  data <- Rsrat::faultdata.nhpp(substitute(time), substitute(fault),
    substitute(type), substitute(te), data, parent.frame())
  con <- Rsrat::srm.nhpp.options()
  nmsC <- names(con)
  con[(namc <- names(control))] <- control
  if (length(noNms <- namc[!namc %in% nmsC]))
  warning("unknown names in control: ", paste(noNms, collapse = ", "))

  if (length(phase) == 1) {
    m <- cphsrm(phase)
    result <- .fit.srm.cph(srm=m, data=data, con=con, ...)
    return(result)
  }
  else {
    result <- lapply(cphsrm(phase), function(m) .fit.srm.cph(srm=m, data=data, con=con, ...))
    if (is.null(selection)) {
      return(result)
    }
    else {
      switch(selection,
             AIC = {
               i <- which.min(sapply(result, function(r) -2*r$llf + 2*r$df))
               result[[i]]
             }
      )
    }
  }
}

.fit.srm.cph <- function(srm, data, con, ...) {
  pnames <- names(srm$params)
  tres <- system.time(result <- emfit(srm, data, initialize = TRUE,
    maxiter = con$maxiter, reltol = con$reltol, abstol = con$abstol,
    trace=con$trace, printsteps=con$printsteps))
  result <- c(result, list(aic=-2*result$llf+2*result$df, ctime=tres[1], call=call))
  names(result$srm$params) <- pnames
  class(result) <- "srm.nhpp.result"
  result
}

#' Software reliability model
#'
#' Generate a software reliability model.
#'
#' Return a class of CPHSRM
#'
#' @param phase A numeric vector indicating the number of phases
#' @return A class of CF1 (\code{\link{CPHSRM}}).
#' @export

cphsrm <- function(phase) {
  if (length(phase) == 1L) {
    .create.cph(phase)
  }
  else {
    names <- lapply(phase, function(n) .create.cph.name(n))
    result <- lapply(phase, function(n) .create.cph(n))
    names(result) <- names
    result
  }
}

.create.cph <- function(p) {
  if (p == 1) {
    Rsrat::ExpSRM$new()
  } else {
    CPHSRM$new(p)
  }
}

.create.cph.name <- function(p) {
  if (p == 1) {
    "cph1 (exp)"
  } else {
    paste("cph", p, sep = "")
  }
}
