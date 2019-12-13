
/*
 * split.Rule = CT
 */
#include <math.h>
#include "IVTree.h"
#include "IVTreeproto.h"

static double *sums, *wtsums, *treatment_effect;
static double *wts, *trs, *trsums;
static int *countn;
static int *tsplit;
static double *wtsqrsums, *trsqrsums;

int CTinit(int n, double *y[], int maxcat, char **error,
        int *size, int who, double *wt, double *treatment, 
        int bucketnum, int bucketMax, double *train_to_est_ratio) {

    if (who == 1 && maxcat > 0) {
        graycode_init0(maxcat);
        countn = (int *) ALLOC(2 * maxcat, sizeof(int));
        tsplit = countn + maxcat;
        treatment_effect = (double *) ALLOC(8 * maxcat, sizeof(double));
        wts = treatment_effect + maxcat;
        trs = wts + maxcat;
        sums = trs + maxcat;
        wtsums = sums + maxcat;
        trsums = wtsums + maxcat;
        wtsqrsums = trsums + maxcat;
        trsqrsums = wtsqrsums + maxcat;
    }

    *size = 1;
    *train_to_est_ratio = n * 1.0 / ct.NumHonest;

    return 0;
}


void CTss(int n, double *y[], double *value, double *con_mean, double *tr_mean, 
     double *risk, double *wt, double *treatment, double *treatment1, double *IV, double max_y,
     double alpha, double train_to_est_ratio) {

    int i;
    double temp0 = 0., temp1 = 0., twt = 0.; /* sum of the weights */ 
    double ttreat = 0.;
    double effect;
    double tr_var, con_var;
    double con_sqr_sum = 0., tr_sqr_sum = 0.;
    double xz_sum = 0., xy_sum = 0., x_sum = 0., y_sum = 0., z_sum = 0.;
    double yz_sum = 0., xx_sum = 0., yy_sum = 0., zz_sum = 0.;
    double alpha_1 = 0., alpha_0 = 0.; 
    // double beta_1 = 0., beta_0 = 0.;
    double numerator, denominator;

    for (i = 0; i < n; i++) {
        temp1 += *y[i] * wt[i] * treatment[i];
        temp0 += *y[i] * wt[i] * (1 - treatment[i]);
        twt += wt[i];
        ttreat += wt[i] * treatment[i];
        tr_sqr_sum += (*y[i]) * (*y[i]) * wt[i] * treatment[i];
        con_sqr_sum += (*y[i]) * (*y[i]) * wt[i] * (1- treatment[i]);
        xz_sum += *y[i] * IV[i];
        xy_sum += treatment1[i] * IV[i];
        x_sum += IV[i];
        y_sum += treatment1[i];
        z_sum += *y[i];
        yz_sum += *y[i] * treatment1[i];
        xx_sum += IV[i] * IV[i];
        yy_sum += treatment1[i] * treatment1[i];
        zz_sum += *y[i] * *y[i];
    }

    alpha_1 = (n * xz_sum - x_sum * z_sum) / (n * xy_sum - x_sum * y_sum);
    effect = alpha_1;
    alpha_0 = (z_sum - alpha_1 * y_sum) / n;
    // beta_1 = (n * xy_sum - x_sum * y_sum) / (n * xx_sum - x_sum * x_sum);
    // beta_0 = (y_sum - beta_1 * x_sum) / n;

    *tr_mean = temp1 / ttreat;
    *con_mean = temp0 / (twt - ttreat);
    *value = effect;
    
    numerator = (zz_sum + n * alpha_0 * alpha_0 + alpha_1 * alpha_1 * yy_sum - 2 * alpha_0 * z_sum - 2 * alpha_1 * yz_sum + 2 * alpha_0 * alpha_1 * y_sum)/n;
    //denominator = n * beta_0 * beta_0 + beta_1 * beta_1 * xx_sum + y_sum * y_sum / n + 2 * beta_0 * beta_1 * x_sum - 2 * beta_0 * y_sum - 2 * beta_1 * x_sum * y_sum / n;
    denominator = 1 / (xx_sum / n - (x_sum / n) * (x_sum / n)) * (xy_sum / n - x_sum/n * y_sum / n) * (xy_sum / n - x_sum/n * y_sum / n) * n;                           
    //denominator = 1/xx_sum*(xy_sum*xy_sum);             
    *risk = 4 * twt * max_y * max_y - alpha * twt * effect * effect + (1 - alpha) * (1 + train_to_est_ratio) * twt * (numerator / denominator);

    // PARAMETER!    
    if(fabs(n * xy_sum - x_sum * y_sum) <= 0 * n * n){
        Rprintf("Entered CT.c. Invalid IV.\n");
        effect = temp1 / ttreat - temp0 / (twt - ttreat);  
        *value = effect;
        tr_var = tr_sqr_sum / ttreat - temp1 * temp1 / (ttreat * ttreat);
        con_var = con_sqr_sum / (twt - ttreat) - temp0 * temp0 / ((twt - ttreat) * (twt - ttreat));
        *risk = 4 * twt * max_y * max_y - alpha * twt * effect * effect + 
                (1 - alpha) * (1 + train_to_est_ratio) * twt * (tr_var /ttreat  + con_var / (twt - ttreat));
    }
            
}



void CT(int n, double *y[], double *x, int nclass, int edge, double *improve, double *split, 
        int *csplit, double myrisk, double *wt, double *treatment, double *treatment1, double *IV, int minsize, double alpha,
        double train_to_est_ratio) {

    int i, j;
    double temp;
    double left_sum, right_sum;
    double left_tr_sum, right_tr_sum;
    double left_tr, right_tr;
    double left_wt, right_wt;
    int left_n, right_n;
    double best;
    int direction = LEFT;
    int where = 0;
    double node_effect, left_effect, right_effect;
    double left_temp, right_temp;
    int min_node_size = minsize;
    
    double tr_var, con_var;
    double right_sqr_sum, right_tr_sqr_sum, left_sqr_sum, left_tr_sqr_sum;
    double left_tr_var, left_con_var, right_tr_var, right_con_var;

    right_wt = 0.;
    right_tr = 0.;
    right_sum = 0.;
    right_tr_sum = 0.;
    right_sqr_sum = 0.;
    right_tr_sqr_sum = 0.;
    right_n = n;
    double right_xz_sum = 0., right_xy_sum = 0., right_x_sum = 0., right_y_sum = 0., right_z_sum = 0.;
    double left_xz_sum = 0., left_xy_sum = 0., left_x_sum = 0., left_y_sum = 0., left_z_sum = 0.;
    double right_yz_sum = 0., right_xx_sum = 0., right_yy_sum = 0., right_zz_sum = 0.;
    double left_yz_sum = 0., left_xx_sum = 0., left_yy_sum = 0., left_zz_sum = 0.;
    double alpha_1 = 0., alpha_0 = 0.;
    // double beta_1 = 0., beta_0 = 0.;
    double numerator, denominator;
    for (i = 0; i < n; i++) {
        right_wt += wt[i];
        right_tr += wt[i] * treatment[i];
        right_sum += *y[i] * wt[i];
        right_tr_sum += *y[i] * wt[i] * treatment[i];
        right_sqr_sum += (*y[i]) * (*y[i]) * wt[i];
        right_tr_sqr_sum += (*y[i]) * (*y[i]) * wt[i] * treatment[i];
        right_xz_sum += *y[i] * IV[i];
        right_xy_sum += treatment1[i] * IV[i];
        right_x_sum += IV[i];
        right_y_sum += treatment1[i];
        right_z_sum += *y[i];
        right_yz_sum += *y[i] * treatment1[i];
        right_xx_sum += IV[i] * IV[i];
        right_yy_sum += treatment1[i] * treatment1[i];
        right_zz_sum += *y[i] * *y[i];
    }

    alpha_1 = (right_n * right_xz_sum - right_x_sum * right_z_sum) / (right_n * right_xy_sum - right_x_sum * right_y_sum);
    alpha_0 = (right_z_sum - alpha_1 * right_y_sum) / right_n;
    // beta_1 = (right_n * right_xy_sum - right_x_sum * right_y_sum) / (right_n * right_xx_sum - right_x_sum * right_x_sum);
    // beta_0 = (right_y_sum - beta_1 * right_x_sum) / right_n;
    temp = alpha_1;
    numerator = (right_zz_sum + right_n * alpha_0 * alpha_0 + alpha_1 * alpha_1 * right_yy_sum - 2 * alpha_0 * right_z_sum - 2 * alpha_1 * right_yz_sum + 2 * alpha_0 * alpha_1 * right_y_sum)/right_n;
    //denominator = right_n * beta_0 * beta_0 + beta_1 * beta_1 * right_xx_sum + right_y_sum * right_y_sum / right_n + 2 * beta_0 * beta_1 * right_x_sum - 2 * beta_0 * right_y_sum - 2 * beta_1 * right_x_sum * right_y_sum / right_n;
    denominator = 1/ (right_xx_sum / right_n - (right_x_sum / right_n) * (right_x_sum / right_n)) *
                  (right_xy_sum / right_n - right_x_sum/right_n * right_y_sum / right_n) * 
                  (right_xy_sum / right_n - right_x_sum/right_n * right_y_sum / right_n) * right_n;                         
    //denominator = 1/right_xx_sum*(right_xy_sum*right_xy_sum);             
    node_effect = alpha * temp * temp * right_wt - (1 - alpha) * (1 + train_to_est_ratio) * right_wt * (numerator / denominator);
    
    // PARAMETER!        
    if(fabs(right_n * right_xy_sum - right_x_sum * right_y_sum) <= 0 * right_n * right_n){
            Rprintf("Entered CT.c. Invalid IV.\n");
            temp = right_tr_sum / right_tr - (right_sum - right_tr_sum) / (right_wt - right_tr);
            tr_var = right_tr_sqr_sum / right_tr - right_tr_sum * right_tr_sum / (right_tr * right_tr);
            con_var = (right_sqr_sum - right_tr_sqr_sum) / (right_wt - right_tr)
                - (right_sum - right_tr_sum) * (right_sum - right_tr_sum) 
                / ((right_wt - right_tr) * (right_wt - right_tr));
            node_effect = alpha * temp * temp * right_wt - (1 - alpha) * (1 + train_to_est_ratio) 
                * right_wt * (tr_var / right_tr  + con_var / (right_wt - right_tr));
    }
    
    if (nclass == 0) {
        /* continuous predictor */
        left_wt = 0;
        left_tr = 0;
        left_n = 0;
        left_sum = 0;
        left_tr_sum = 0;
        left_sqr_sum = 0;
        left_tr_sqr_sum = 0;
        best = 0;
        for (i = 0; right_n > edge; i++) {
            //Rprintf("Entered Adding.");
            left_wt += wt[i];
            right_wt -= wt[i];
            left_tr += wt[i] * treatment[i];
            right_tr -= wt[i] * treatment[i];
            left_n++;
            right_n--;
            temp = *y[i] * wt[i] * treatment[i];
            left_tr_sum += temp;
            right_tr_sum -= temp;
            left_sum += *y[i] * wt[i];
            right_sum -= *y[i] * wt[i];
            temp = (*y[i]) *  (*y[i]) * wt[i];
            left_sqr_sum += temp;
            right_sqr_sum -= temp;
            temp = (*y[i]) * (*y[i]) * wt[i] * treatment[i];
            left_tr_sqr_sum += temp;
            right_tr_sqr_sum -= temp;
                
            left_xz_sum += *y[i] * IV[i];
            right_xz_sum -= *y[i] * IV[i];
            left_xy_sum += treatment1[i] * IV[i];
            right_xy_sum -= treatment1[i] * IV[i];
            left_x_sum += IV[i];
            right_x_sum -= IV[i];
            left_y_sum += treatment1[i];
            right_y_sum -= treatment1[i];
            left_z_sum += *y[i];
            right_z_sum -= *y[i];
            left_yz_sum += *y[i] * treatment1[i];
            right_yz_sum -= *y[i] * treatment1[i];
            left_xx_sum += IV[i] * IV[i];
            right_xx_sum -= IV[i] * IV[i];
            left_yy_sum += treatment1[i] * treatment1[i];
            right_yy_sum -= treatment1[i] * treatment1[i];
            left_zz_sum += *y[i] * *y[i];
            right_zz_sum -= *y[i] * *y[i];
            //Rprintf("Entered CT.c. left_n, left_wt, left_tr, min_node_size, edge are %.2d, %.2f, %.2f, %.2d, %.2d.\n", left_n, left_wt, left_tr, min_node_size, edge);
            if (x[i + 1] != x[i] && left_n >= edge &&
                (int) left_tr >= min_node_size &&
                (int) left_wt - (int) left_tr >= min_node_size &&
                (int) right_tr >= min_node_size &&
                (int) right_wt - (int) right_tr >= min_node_size) {                                                       
                alpha_1 = (left_n * left_xz_sum - left_x_sum * left_z_sum) / (left_n * left_xy_sum - left_x_sum * left_y_sum);
                alpha_0 = (left_z_sum - alpha_1 * left_y_sum) / left_n;
                // beta_1 = (left_n * left_xy_sum - left_x_sum * left_y_sum) / (left_n * left_xx_sum - left_x_sum * left_x_sum);
                // beta_0 = (left_y_sum - beta_1 * left_x_sum) / left_n;
                left_temp = alpha_1;
                numerator = (left_zz_sum + left_n * alpha_0 * alpha_0 + alpha_1 * alpha_1 * left_yy_sum - 2 * alpha_0 * left_z_sum - 2 * alpha_1 * left_yz_sum + 2 * alpha_0 * alpha_1 * left_y_sum)/left_n;
                //denominator = left_n * beta_0 * beta_0 + beta_1 * beta_1 * left_xx_sum + left_y_sum * left_y_sum / left_n + 2 * beta_0 * beta_1 * left_x_sum - 2 * beta_0 * left_y_sum - 2 * beta_1 * left_x_sum * left_y_sum / left_n;
                //Rprintf("Entered CT.c. Left num, den, effect, variance and obs are %.fd, %.2f, %.2f, %.2f, %.2f.\n", numerator, denominator, left_temp * left_temp, (numerator / denominator/left_wt), left_wt);
                
                denominator = 1/(left_xx_sum / left_n - (left_x_sum / left_n) * (left_x_sum / left_n)) *
                              (left_xy_sum / left_n - left_x_sum/left_n * left_y_sum / left_n) * 
                              (left_xy_sum / left_n - left_x_sum/left_n * left_y_sum / left_n) * left_n;     
                //denominator = 1/left_xx_sum*(left_xy_sum*left_xy_sum); 
                        
                //Rprintf("Entered CT.c. Left num, den, effect, variance and obs are %.2f, %.2f, %.2f, %.2f, %.2f.\n", numerator, denominator, left_temp * left_temp, (numerator / denominator), left_wt);
             
                left_effect = alpha * left_temp * left_temp * left_wt - (1 - alpha) * (1 + train_to_est_ratio) 
                    * left_wt * (numerator / denominator);
                //Rprintf("Entered CT.c. Left a1, a0, b1, b0 are %.2f, %.2f, %.2f, %.2f.\n", alpha_1, alpha_0, beta_1, beta_0);
                //Rprintf("Entered CT.c. Left treatment effect, num, den, effect and obs are %.2f, %.2f, %.2f, %.2f, %.2f.\n", left_temp, numerator, denominator, left_effect, left_wt);
                
                // PARAMETER!                    
                if(fabs(left_n * left_xy_sum - left_x_sum * left_y_sum) <= 0 * left_n * left_n){
                    Rprintf("Entered CT.c. Invalid IV.\n");
                    left_temp = left_tr_sum / left_tr - (left_sum - left_tr_sum) / (left_wt - left_tr);
                    left_tr_var = left_tr_sqr_sum / left_tr - 
                        left_tr_sum  * left_tr_sum / (left_tr * left_tr);
                    left_con_var = (left_sqr_sum - left_tr_sqr_sum) / (left_wt - left_tr)  
                        - (left_sum - left_tr_sum) * (left_sum - left_tr_sum)
                        / ((left_wt - left_tr) * (left_wt - left_tr));        
                    left_effect = alpha * left_temp * left_temp * left_wt
                            - (1 - alpha) * (1 + train_to_est_ratio) * left_wt 
                        * (left_tr_var / left_tr + left_con_var / (left_wt - left_tr));
                }
                

                alpha_1 = (right_n * right_xz_sum - right_x_sum * right_z_sum) / (right_n * right_xy_sum - right_x_sum * right_y_sum);
                alpha_0 = (right_z_sum - alpha_1 * right_y_sum) / right_n;
                // beta_1 = (right_n * right_xy_sum - right_x_sum * right_y_sum) / (right_n * right_xx_sum - right_x_sum * right_x_sum);
                // beta_0 = (right_y_sum - beta_1 * right_x_sum) / right_n;
                right_temp = alpha_1;
                numerator = (right_zz_sum + right_n * alpha_0 * alpha_0 + alpha_1 * alpha_1 * right_yy_sum - 2 * alpha_0 * right_z_sum - 2 * alpha_1 * right_yz_sum + 2 * alpha_0 * alpha_1 * right_y_sum)/right_n;
                //denominator = right_n * beta_0 * beta_0 + beta_1 * beta_1 * right_xx_sum + right_y_sum * right_y_sum / right_n + 2 * beta_0 * beta_1 * right_x_sum - 2 * beta_0 * right_y_sum - 2 * beta_1 * right_x_sum * right_y_sum / right_n;
                denominator = 1/(right_xx_sum / right_n - (right_x_sum / right_n) * (right_x_sum / right_n)) *
                              (right_xy_sum / right_n - right_x_sum/right_n * right_y_sum / right_n) * 
                              (right_xy_sum / right_n - right_x_sum/right_n * right_y_sum / right_n) * right_n;  
                //denominator = 1/right_xx_sum*(right_xy_sum*right_xy_sum); 
                    
                //Rprintf("Entered CT.c. Left num, den, effect, variance and obs are %.2f, %.2f, %.2f, %.2f, %.2f.\n", numerator, denominator, right_temp * right_temp, (numerator / denominator), right_wt);

                right_effect = alpha * right_temp * right_temp * right_wt - (1 - alpha) * (1 + train_to_est_ratio) 
                     * right_wt * (numerator / denominator);
                //Rprintf("Entered CT.c. Right treatment effect, num, dem, effect and obs are %.2f, %.2f, %.2f, %.2f, %.2f.\n", right_temp, numerator, denominator, right_effect, right_wt);

                // PARAMETER!                    
                if(fabs(right_n * right_xy_sum - right_x_sum * right_y_sum) <= 0 * right_n * right_n){
                    Rprintf("Entered CT.c. Invalid IV.\n");
                    right_temp = right_tr_sum / right_tr - (right_sum - right_tr_sum) / (right_wt - right_tr);
                    right_tr_var = right_tr_sqr_sum / right_tr -
                        right_tr_sum * right_tr_sum / (right_tr * right_tr);
                    right_con_var = (right_sqr_sum - right_tr_sqr_sum) / (right_wt - right_tr)
                        - (right_sum - right_tr_sum) * (right_sum - right_tr_sum) 
                        / ((right_wt - right_tr) * (right_wt - right_tr));
                    right_effect = alpha * right_temp * right_temp * right_wt
                            - (1 - alpha) * (1 + train_to_est_ratio) * right_wt * 
                                (right_tr_var / right_tr + right_con_var / (right_wt - right_tr));
                }


                temp = left_effect + right_effect - node_effect;
                if (temp > best) {
                    best = temp;
                //Rprintf("Improved. Left, right, node effects and best are %.2f, %.2f, %.2f, %.2f.\n", left_effect, right_effect, node_effect, best);
                    where = i;               
                    if (left_temp < right_temp){
                        direction = LEFT;
                    }
                    else{
                //Rprintf("Not Improved. Left, right and node effects are %.2f, %.2f, %.2f, %.2f.\n", left_effect, right_effect, node_effect, best);
                        direction = RIGHT;
                    }
                }             
            }
        }
        
        *improve = best;
        if (best > 0) {         /* found something */
        csplit[0] = direction;
            *split = (x[where] + x[where + 1]) / 2; 
        }
    }
    
    /*
    * Categorical predictor
    */
    else {
        for (i = 0; i < nclass; i++) {
            countn[i] = 0;
            wts[i] = 0;
            trs[i] = 0;
            sums[i] = 0;
            wtsums[i] = 0;
            trsums[i] = 0;
            wtsqrsums[i] = 0;
            trsqrsums[i] = 0;
        }
        
        /* rank the classes by treatment effect */
        for (i = 0; i < n; i++) {
            j = (int) x[i] - 1;
            countn[j]++;
            wts[j] += wt[i];
            trs[j] += wt[i] * treatment[i];
            sums[j] += *y[i];
            wtsums[j] += *y[i] * wt[i];
            trsums[j] += *y[i] * wt[i] * treatment[i];
            wtsqrsums[j] += (*y[i]) * (*y[i]) * wt[i];
            trsqrsums[j] +=  (*y[i]) * (*y[i]) * wt[i] * treatment[i];
        }
        
        for (i = 0; i < nclass; i++) {
            if (countn[i] > 0) {
                tsplit[i] = RIGHT;
                treatment_effect[i] = trsums[j] / trs[j] - (wtsums[j] - trsums[j]) / (wts[j] - trs[j]);
            } else
                tsplit[i] = 0;
        }
        
        graycode_init2(nclass, countn, treatment_effect);
        
        /*
         * Now find the split that we want
         */
        
        left_wt = 0;
        left_tr = 0;
        left_n = 0;
        left_sum = 0;
        left_tr_sum = 0;
        left_sqr_sum = 0.;
        left_tr_sqr_sum = 0.;
        
        best = 0;
        where = 0;
        while ((j = graycode()) < nclass) {
            tsplit[j] = LEFT;
            left_n += countn[j];
            right_n -= countn[j];
            
            left_wt += wts[j];
            right_wt -= wts[j];
            
            left_tr += trs[j];
            right_tr -= trs[j];
            
            left_sum += wtsums[j];
            right_sum -= wtsums[j];
            
            left_tr_sum += trsums[j];
            right_tr_sum -= trsums[j];
            
            left_sqr_sum += wtsqrsums[j];
            right_sqr_sum -= wtsqrsums[j];
            
            left_tr_sqr_sum += trsqrsums[j];
            right_tr_sqr_sum -= trsqrsums[j];
            
            if (left_n >= edge && right_n >= edge &&
                (int) left_tr >= min_node_size &&
                (int) left_wt - (int) left_tr >= min_node_size &&
                (int) right_tr >= min_node_size &&
                (int) right_wt - (int) right_tr >= min_node_size) {
                
                left_temp = left_tr_sum / left_tr - (left_sum - left_tr_sum) 
                    / (left_wt - left_tr);
                
                left_tr_var = left_tr_sqr_sum / left_tr 
                    - left_tr_sum  * left_tr_sum / (left_tr * left_tr);
                left_con_var = (left_sqr_sum - left_tr_sqr_sum) / (left_wt - left_tr)  
                    - (left_sum - left_tr_sum) * (left_sum - left_tr_sum)
                    / ((left_wt - left_tr) * (left_wt - left_tr));       
                left_effect = alpha * left_temp * left_temp * left_wt
                    - (1 - alpha) * (1 + train_to_est_ratio) * left_wt * 
                        (left_tr_var / left_tr + left_con_var / (left_wt - left_tr));
                
                right_temp = right_tr_sum / right_tr - (right_sum - right_tr_sum) 
                    / (right_wt - right_tr);
                right_tr_var = right_tr_sqr_sum / right_tr 
                    - right_tr_sum * right_tr_sum / (right_tr * right_tr);
                right_con_var = (right_sqr_sum - right_tr_sqr_sum) / (right_wt - right_tr)
                    - (right_sum - right_tr_sum) * (right_sum - right_tr_sum) 
                    / ((right_wt - right_tr) * (right_wt - right_tr));
                right_effect = alpha * right_temp * right_temp * right_wt
                        - (1 - alpha) * (1 + train_to_est_ratio) * right_wt *
                            (right_tr_var / right_tr + right_con_var / (right_wt - right_tr));
                temp = left_effect + right_effect - node_effect;
            
                
                if (temp > best) {
                    best = temp;
                    
                    if (left_temp > right_temp)
                        for (i = 0; i < nclass; i++) csplit[i] = -tsplit[i];
                    else
                        for (i = 0; i < nclass; i++) csplit[i] = tsplit[i];
                }
            }
        }
        *improve = best;
    }
}


double CTpred(double *y, double wt, double treatment, double *yhat, double propensity) {

        double ystar;
        double temp;
        
        ystar = y[0] * (treatment - propensity) / (propensity * (1 - propensity));
        temp = ystar - *yhat;
        return temp * temp * wt;
}
