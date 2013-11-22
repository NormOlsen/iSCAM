/**
Logistic-normal likelihood for size-age composition data.
Much of this code is based on a paper written by Chris Francis

Author: Steve Martell
Date  : Nov 22, 2013.

Descrition:
	Let  Y = number of years of composition information (rows)
	let  B = number of age/size bins in the composition matrix (cols)

	---- CONSTRUCTOR ----
	The constructor requires and observed (O) and expected (E) matrix
	of composition data.  These data are immediately transformed into
	proportions that sum to 1 in each year.

	I used Aitchison (2003) approach where vector of observations
	was transformed as:

		O_b = exp(Y_b)/(1+sum_b^{B-1} exp(Y_b)) , for b = 1, ..., B-1
		O_b = 1 - sum_b^{B-1} O_b               , for b = B

	Then I use the aggregate arrays function to almalgamate 0s and perform
	tail compression.  An member array of residuals m_w is created in this 
	function where: 

		m_w = ln(O_b/O_B) - ln(E_b/E_B)

	Note that each m_w vector (year) is of length (B-1).

	---- LIKELIHOOD ----
	For the simplist likelihood with no correlation and an estimated variance 
	tau2, the likelihood for each year is given by:

	nll = 0.5(B-1)*ln(2*\pi) + sum(ln(O_b)) + 0.5*ln(|V|) + 
	      (B-1)*ln(W_y) + 0.5* m_w^2/(V*W_y^2)

	Where Wy is the year effect of weight based on relative sample size:

	W_y = [mean(N)/N_y]^0.5

	And V is the covariance matrix which is just a simple scaler tau2.



**/


#include <admodel.h>
#include "logistic_normal.h"

logistic_normal::~logistic_normal()
{}

logistic_normal::logistic_normal()
{}

logistic_normal::logistic_normal(const dmatrix& _O, const dvar_matrix& _E)
: m_O(_O), m_E(_E)
{
	/*
	O - observed numbers-at-age or proportions-at-age.
	E - expected numbers-at-age or porportions-at-age.

	Matrix rows correspond to years, cols age or length bins.
	*/
	// cout<<"Entered the constructor"<<endl;
	m_b1 = m_O.colmin();
	m_b2 = m_O.colmax();
	m_B  = m_b2 - m_b1 + 1;

	m_y1 = m_O.rowmin();
	m_y2 = m_O.rowmax();
	m_Y  = m_y2 - m_y1 + 1;

	m_dWy.allocate(m_y1,m_y2);
	m_dWy.initialize();

	m_Op.allocate(m_y1,m_y2,m_b1,m_b2);
	m_Ep.allocate(m_y1,m_y2,m_b1,m_b2);
	m_Ox.allocate(m_y1,m_y2,m_b1,m_b2);
	m_Ex.allocate(m_y1,m_y2,m_b1,m_b2);
	m_Oz.allocate(m_y1,m_y2,m_b1,m_b2);
	m_Ez.allocate(m_y1,m_y2,m_b1,m_b2);

	int i;
	int l = m_b1;
	int bm1 = m_b2-1;
	for( i = m_y1; i <= m_y2; i++ )
	{
		// Ensure proportions-at-age/size sume to 1.
		m_Op(i) = m_O(i) / sum( m_O(i) );
		m_Ep(i) = m_E(i) / sum( m_E(i) );

		// Logistic transformation using Aitichson's approach
		m_Ox(i)(l,bm1) = exp(m_Op(i)(l,bm1)) / (1 + sum(exp(m_Op(i)(l,bm1))));
		m_Ex(i)(l,bm1) = exp(m_Ep(i)(l,bm1)) / (1 + sum(exp(m_Ep(i)(l,bm1))));
		m_Ox(i)(m_b2)  = 1. - sum(m_Ox(i)(l,bm1));
		m_Ex(i)(m_b2)  = 1. - sum(m_Ex(i)(l,bm1));

		m_Oz(i) = exp(m_Op(i)) / sum(exp(m_Op(i)));
		m_Ez(i) = exp(m_Ep(i)) / sum(exp(m_Ep(i)));
	}
	
	// Calculate mean weighting parameters for each year.
	dvector dNy = rowsum(m_O);	
	double dN   = mean(dNy);
	m_dWy       = sqrt( dN / dNy );

	
	// Minimum proportion to pool into adjacent cohort.
	m_dMinimumProportion = 0;
	m_rho = 0.1;
	aggregate_arrays();
	cout<<"Ok at the end of the constructor"<<endl;

}


dvariable logistic_normal::negative_loglikelihood(const dvariable& tau2)
{
	m_nll = 0;

	int i;
	m_sig2=tau2;

	for( i = m_y1; i <= m_y2; i++ )
	{	
		int    nB     = m_nNminp(i);
		double t1     = 0.5 * (nB - 1.0) * log(2.0*PI);
		double t3     = sum( log(m_Oa(i)) );
		dvariable det = pow(nB * m_sig2, 2.*(nB-1.));
		dvariable t5  = 0.5 * log(det);
		double t7     = (nB - 1.0) * log(m_dWy(i));
		dvariable t9  = 0.5 * sum( square(m_w(i)) / (m_sig2 * square(m_dWy(i))) );
		
		m_nll        += t1 + t3 + t5 + t7 + t9;
	}
		
	return(m_nll);
}

dvariable logistic_normal::negative_loglikelihood()
{
	// negative loglikelihood evaluated at the MLE of the variance tau2.
	m_nll = 0;
	cout<<"Integrate over the variance"<<endl;
	int i;
	correlation_matrix();
	m_sig2 = 0;
	dvariable sws = 0;
	for( i = m_y1; i <= m_y2; i++ )
	{
		cout<<m_w(i)<<endl<<endl;
		cout<<m_V(i)<<endl<<endl;
		cout<<m_w(i)*m_V(i)<<endl<<endl;
		cout<<(m_w(i)*m_V(i))*m_w(i)<<endl;
		sws += (m_w(i) * m_V(i)) * m_w(i);
		exit(1);
	}

	double t1 = 0.5 * log(2.0*PI) * sum( m_nNminp-1 );
	double t2 = sum( log(m_Oa) );

	return(m_nll);
}

dvar_matrix logistic_normal::standardized_residuals()
{
	/*
		Standardized residuals
		nu = [log(O/~O) - log(E/~E)]/(Wy m_sig2^0.5)
		where O & E are the observed and expected proportion vectors
		~O and ~E is the geometric means of each proporition vectors
		Wy is the relative weight each year.
		
	*/
	int i,j;
	double    tilde_O;
	dvariable tilde_E;
	m_residual.allocate(m_Op);
	m_residual.initialize();
	for( i = m_y1; i <= m_y2; i++ )
	{
		dvector     oo = m_Oa(i);
		dvar_vector pp = m_Ea(i);
		tilde_O = exp( mean(log(oo)) );
		tilde_E = exp( mean(log(pp)) );

		dvar_vector	w = log(oo/tilde_O) - log(pp/tilde_E);
		w            /= m_sig2/m_dWy(i);
		
		for( j = 1; j <= m_nNminp(i); j++ )
		{
			int idx = m_nAgeIndex(i,j);
			m_residual(i)(idx) = w(j);
		}
		
	}
	
	return(m_residual);
}


void logistic_normal::aggregate_arrays()
{
	/*
		- Aggregate minimum proportions in to adjacent cohorts (tail compression)
		- This routine populates m_Oa and m_Ea as ragged objects to be used for
		  likelihood calculations.
	*/
	m_nNminp.allocate(m_y1,m_y2);
	

	int k;
	int nB;
	for(int i = m_y1; i <= m_y2; i++ )
	{
		int n = 0;  // number of non-zero observations in each year
		for(int j = m_b1; j <= m_b2; j++ )
		{
			if( m_Op(i,j) > m_dMinimumProportion )
			{
				n ++;
			}
		}
		m_nNminp(i) = n;
	}

	// Ragged arrays with tail compression and zeros omitted.
	m_nAgeIndex.allocate(m_y1,m_y2,1,m_nNminp);
	m_Oa.allocate(m_y1,m_y2,1,m_nNminp);
	m_Ea.allocate(m_y1,m_y2,1,m_nNminp);
	m_w.allocate(m_y1,m_y2,1,m_nNminp-1);
	
	m_nAgeIndex.initialize();
	m_Oa.initialize();
	m_Ea.initialize();
	m_w.initialize();

	for(int i = m_y1; i <= m_y2; i++ )
	{
		dvector     oo = m_Op(i);
		dvar_vector pp = m_Ep(i);
		k = 1;
		for(int j = m_b1; j <= m_b2; j++ )
		{
			if( oo(j) <= m_dMinimumProportion )
			{
				m_Oa(i,k) += oo(j);
				m_Ea(i,k) += pp(j);
			}
			else
			{
				m_Oa(i,k) += oo(j);
				m_Ea(i,k) += pp(j);

				if(k <=m_nNminp(i)) m_nAgeIndex(i,k) = j;
				if(k < m_nNminp(i)) k++;
			}
		}

		// compressed matrix of log-residuals
		nB = m_nNminp(i);
		m_w(i) = log(m_Oa(i)(1,nB-1)/m_Oa(i,nB))
		        -log(m_Ea(i)(1,nB-1)/m_Ea(i,nB));
		
	}
}


void logistic_normal::correlation_matrix()
{
	// Calculate the covariance matrix for each year, based on ragged arrays m_w;

	int i,j,k;
	m_V.allocate(m_y1,m_y2,1,m_nNminp,1,m_nNminp);
	m_V.initialize();

	for( i = m_y1; i <= m_y2; i++ )
	{
		 dmatrix K = identity_matrix(1,m_nNminp(i));
		 dvar_matrix C(1,m_nNminp(i),1,m_nNminp(i));
		 for( j = 1; j <= m_nNminp(i); j++ )
		 {
		 	 for( k = 1; k <= m_nNminp(i); k++ )
		 	 {
		 	 	C(j,k) = pow(m_rho,abs(double(j)-double(k)));
		 	 }
		 }
		 m_V(i) = K * C * trans(K);
	}	

}













