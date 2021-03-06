#include <cmath>
#include <cstdint>

#include <Rcpp.h>
using namespace Rcpp;

// Enable C++11 via this plugin (Rcpp 0.10.3 or later)
// [[Rcpp::plugins(cpp11)]]

#include "array.h"
#include "poisson.h"
#include "blas1.h"
#include "blas2.h"
#include "ctmc.h"
#include "cf1utils.h"

using namespace marlib;

//' @rdname cf1
//' @param dx A numeric value of time differences
//' @param alpha A vector of initial probabilities
//' @param rate A vector of transition rates
//' @param eps A value of tolerance error
//' @param ufactor A value of uniformization factor
//' @export
// [[Rcpp::export]]

NumericVector C_cf1pdf(NumericVector dx,
                     NumericVector alpha,
                     NumericVector rate,
                    double eps = 1.0e-8,
                    double ufactor = 1.01,
                    bool log = false) {
  const int k = dx.length();
  const double tmax = dx[idamax(dx)];
  const int n = alpha.length();
  NumericVector P = clone(rate);
  double qv = unif(cf1_matrix(), P, ufactor);

  NumericVector result(k);
  NumericVector prob(rightbound(qv*tmax, eps)+1);  
  NumericVector tmp = clone(alpha);
  NumericVector xi(n);
  for (int i=0; i<k; i++) {
    int right = rightbound(qv*dx[i], eps);
    double weight = pmf(qv*dx[i], 0, right, prob);
    mexpv(cf1_matrix(), trans(), P, prob, right, weight, tmp, tmp, xi);
    result[i] = rate[n-1] * tmp[n-1];
  }
  if (log == true) {
    return Rcpp::log(result);
  } else {
    return result;
  }
}

//' @rdname cf1
//' @export
// [[Rcpp::export]]

NumericVector C_cf1cdf(NumericVector dx,
                     NumericVector alpha,
                     NumericVector rate,
                     double eps = 1.0e-8,
                     double ufactor = 1.01,
                     bool lower = true,
                     bool log = false) {
  const int k = dx.length();
  const double tmax = dx[idamax(dx)];
  const int n = alpha.length();
  NumericVector P = clone(rate);
  double qv = unif(cf1_matrix(), P, ufactor);

  NumericVector result(k);
  NumericVector prob(rightbound(qv*tmax, eps)+1);
  NumericVector tmp = clone(alpha);
  NumericVector xi(n);
  for (int i=0; i<k; i++) {
    int right = rightbound(qv*dx[i], eps);
    double weight = pmf(qv*dx[i], 0, right, prob);
    mexpv(cf1_matrix(), trans(), P, prob, right, weight, tmp, tmp, xi);
    result[i] = dasum(tmp);
  }
  if (lower == false && log == false) {
    return result;
  } else if (lower == true && log == false) {
    return 1 - result;
  } else if (lower == false && log == true) {
    return Rcpp::log(result);
  } else {// if (lower == false && log == true) {
    return Rcpp::log(1-result);
  }
}

//' @rdname cf1
//' @export
// [[Rcpp::export]]

NumericVector C_cf1sample(int n,
                          NumericVector alpha,
                          NumericVector rate) {
  NumericVector res(n); // res should be cleared by zero
  int y = 0;
  double prob = 1.0;
  for (int l=0; l<alpha.size(); l++) {
    y += R::rbinom(n-y, alpha[l]/prob);
    prob -= alpha[l];  // TODO: Does it change to plus?
    for (int i=0; i<y; i++) {
      res[i] += -std::log(R::runif(0,1)) / rate[l]; // R::rexp may require the mean parameter.
    }
  }
  return res;
}

//' @rdname cf1
//' @export
// [[Rcpp::export]]

List C_cf1reform(NumericVector alpha, NumericVector rate) {
  NumericVector a = clone(alpha);
  NumericVector r = clone(rate);
  cf1_sort(a, r);
  return List::create(Named("alpha")=a, Named("rate")=r);
}

//' @rdname cf1
//' @export
// [[Rcpp::export]]

NumericVector C_cf1sojourn(NumericVector alpha, NumericVector rate,
                           NumericVector f, NumericVector b,
                           double t,
                           double eps = 1.0e-8,
                           double ufactor = 1.01) {
  const int n = alpha.length();
  NumericVector P = clone(rate);
  double qv = unif(cf1_matrix(), P, ufactor);

  int right = rightbound(qv*t, eps);
  NumericVector prob(right+1);
  double weight = pmf(qv*t, 0, right+1, prob);

  NumericVector H(n+n);
  NumericVector f2 = clone(f);
  NumericVector xi(n);
  tmp_array2 vc(right+2, n);
  mexp_conv(cf1_matrix(), trans(), P, qv, prob, right, weight,
            f2, b, f2, H, xi, vc);
  return H;
}

//' EM step
//'
//' Execute one EM step for software reliability models.
//'
//' @param params A numeric vector for model parameters
//' @param data A faultdata
//' @param eps A numeric value for tolerance error.
//' @param ufactor A numeric value indicating the uniformization factor
//' @return A list with an updated parameter vector (param),
//' absolute difference of parameter vector (pdiff),
//' log-likelihood function for a given parameter vector (llf),
//' the number of total faults (total).
//' @export
// [[Rcpp::export]]

List em_cf1_emstep(List params, List data, double eps = 1.0e-8, double ufactor = 1.01) {
  const double omega = as<double>(params["omega"]);
  NumericVector alpha = as<NumericVector>(params["alpha"]);
  NumericVector rate = as<NumericVector>(params["rate"]);
  NumericVector time = as<NumericVector>(data["time"]);
  IntegerVector num = as<IntegerVector>(data["fault"]);
  IntegerVector type = as<IntegerVector>(data["type"]);
  
  const int n = alpha.length();
  double new_omega = 0.0;
  NumericVector new_alpha(n);
  NumericVector new_rate(n);
  double llf = cf1emstep(omega, alpha, rate,
                         new_omega, new_alpha, new_rate,
                         time, num, type, eps, ufactor);
  cf1_sort(new_alpha, new_rate);
  return List::create(
    Named("param") = List::create(
      Named("omega")=new_omega,
      Named("alpha")=new_alpha,
      Named("rate")=new_rate
    ),
    Named("pdiff") = List::create(
      Named("omega")=new_omega - omega,
      Named("alpha")=new_alpha - alpha,
      Named("rate")=new_rate - rate
    ),
    Named("llf") = llf,
    Named("total") = new_omega
  );
}
