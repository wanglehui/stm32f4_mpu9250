/*
The MIT License (MIT)

Copyright (c) 2015-? suhetao

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "CKF.h"
#include "FastMath.h"

//////////////////////////////////////////////////////////////////////////
//all parameters below need to be tune including the weight
#define CKF_PQ_INITIAL 0.000001
#define CKF_PW_INITIAL 0.000001

#define CKF_QQ_INITIAL 0.000045
#define CKF_QW_INITIAL 0.00174532925199432957692369076849f

#define CKF_RQ_INITIAL 0.000001
#define CKF_RA_INITIAL 0.48828125
#define CKF_RW_INITIAL 0.00174532925199432957692369076849f
#define CKF_RM_INITIAL 0.105
//////////////////////////////////////////////////////////////////////////
//
void CKF_New(CKF_Filter* ukf)
{
	float32_t *P = ukf->P_f32;
	float32_t *Q = ukf->Q_f32;
	float32_t *R = ukf->R_f32;
	//////////////////////////////////////////////////////////////////////////
	//initialise kesi
	//generate the cubature point
	float32_t kesi = (float32_t)CKF_STATE_DIM;
	arm_matrix_instance_f32 KesiPuls, KesiMinu;
	float32_t KesiPuls_f32[CKF_STATE_DIM * CKF_STATE_DIM],
		KesiMinus_f32[CKF_STATE_DIM * CKF_STATE_DIM];

	arm_mat_init_f32(&KesiPuls, CKF_STATE_DIM, CKF_STATE_DIM, KesiPuls_f32);
	arm_mat_init_f32(&KesiMinu, CKF_STATE_DIM, CKF_STATE_DIM, KesiMinus_f32);
	arm_sqrt_f32(kesi, &kesi);
	arm_mat_identity_f32(&KesiPuls, kesi);
	arm_mat_identity_f32(&KesiMinu, -kesi);
	arm_mat_init_f32(&ukf->Kesi, CKF_STATE_DIM, CKF_CP_POINTS, ukf->Kesi_f32);
	arm_mat_setsubmatrix_f32(&ukf->Kesi, &KesiPuls, 0, 0);
	arm_mat_setsubmatrix_f32(&ukf->Kesi, &KesiMinu, 0, CKF_STATE_DIM);
	arm_mat_init_f32(&ukf->iKesi, CKF_STATE_DIM, 1, ukf->iKesi_f32);

	//initialise weight
	ukf->W = 1.0f / (float32_t)CKF_CP_POINTS;

	//initialise P
	arm_mat_init_f32(&ukf->P, CKF_STATE_DIM, CKF_STATE_DIM, ukf->P_f32);
	P[0] = P[8] = P[16] = P[24] = CKF_PQ_INITIAL;
	P[32] = P[40] = P[48] = CKF_PW_INITIAL;

	arm_mat_init_f32(&ukf->PX, CKF_STATE_DIM, CKF_STATE_DIM, ukf->PX_f32);
	arm_mat_init_f32(&ukf->PY, CKF_MEASUREMENT_DIM, CKF_MEASUREMENT_DIM, ukf->PY_f32);
	arm_mat_init_f32(&ukf->tmpPY, CKF_MEASUREMENT_DIM, CKF_MEASUREMENT_DIM, ukf->tmpPY_f32);
	arm_mat_init_f32(&ukf->PXY, CKF_STATE_DIM, CKF_MEASUREMENT_DIM, ukf->PXY_f32);
	arm_mat_init_f32(&ukf->tmpPXY, CKF_STATE_DIM, CKF_MEASUREMENT_DIM, ukf->tmpPXY_f32);
	//initialise Q
	arm_mat_init_f32(&ukf->Q, CKF_STATE_DIM, CKF_STATE_DIM, ukf->Q_f32);
	Q[0] = Q[8] = Q[16] = Q[24] = CKF_QQ_INITIAL;
	Q[32] = Q[40] = Q[48] = CKF_QW_INITIAL;
	//initialise R
	arm_mat_init_f32(&ukf->R, CKF_MEASUREMENT_DIM, CKF_MEASUREMENT_DIM, ukf->R_f32);
	R[0] = R[14] = R[28] = R[42] = CKF_RQ_INITIAL;
	R[56] = R[70] = R[84] = CKF_RA_INITIAL;
	R[98] = R[112] = R[126] = CKF_RW_INITIAL;
	R[140] = R[154] = R[168] = CKF_RM_INITIAL;

	//other stuff
	arm_mat_init_f32(&ukf->XCP, CKF_STATE_DIM, CKF_CP_POINTS, ukf->XCP_f32);
	arm_mat_init_f32(&ukf->XminusCP, CKF_STATE_DIM, CKF_CP_POINTS, ukf->XminusCP_f32);
	arm_mat_init_f32(&ukf->YCP, CKF_MEASUREMENT_DIM, CKF_CP_POINTS, ukf->YSP_f32);
	//////////////////////////////////////////////////////////////////////////
	arm_mat_init_f32(&ukf->K, CKF_STATE_DIM, CKF_MEASUREMENT_DIM, ukf->K_f32);
	arm_mat_init_f32(&ukf->KT, CKF_MEASUREMENT_DIM, CKF_STATE_DIM, ukf->KT_f32);
	//////////////////////////////////////////////////////////////////////////
	arm_mat_init_f32(&ukf->X, CKF_STATE_DIM, 1, ukf->X_f32);
	arm_mat_zero_f32(&ukf->X);
	arm_mat_init_f32(&ukf->XT, 1, CKF_STATE_DIM, ukf->XT_f32);
	arm_mat_zero_f32(&ukf->XT);
	
	arm_mat_init_f32(&ukf->Xminus, CKF_STATE_DIM, 1, ukf->Xminus_f32);
	arm_mat_zero_f32(&ukf->Xminus);
	arm_mat_init_f32(&ukf->XminusT, 1, CKF_STATE_DIM, ukf->XminusT_f32);
	arm_mat_zero_f32(&ukf->XminusT);
	
	arm_mat_init_f32(&ukf->tmpX, CKF_STATE_DIM, 1, ukf->tmpX_f32);
	arm_mat_zero_f32(&ukf->tmpX);
	//////////////////////////////////////////////////////////////////////////
	arm_mat_init_f32(&ukf->Y, CKF_MEASUREMENT_DIM, 1, ukf->Y_f32);
	arm_mat_zero_f32(&ukf->Y);
	arm_mat_init_f32(&ukf->YT, 1, CKF_MEASUREMENT_DIM, ukf->YT_f32);
	arm_mat_zero_f32(&ukf->YT);
	
	arm_mat_init_f32(&ukf->Yminus, CKF_MEASUREMENT_DIM, 1, ukf->Yminus_f32);
	arm_mat_zero_f32(&ukf->Yminus);
	arm_mat_init_f32(&ukf->YminusT, 1, CKF_MEASUREMENT_DIM, ukf->YminusT_f32);
	arm_mat_zero_f32(&ukf->YminusT);
	
	arm_mat_init_f32(&ukf->tmpY, CKF_MEASUREMENT_DIM, 1, ukf->tmpY_f32);
	arm_mat_zero_f32(&ukf->tmpY);
	//////////////////////////////////////////////////////////////////////////
}

void CKF_Init(CKF_Filter* ukf, float32_t *q, float32_t *gyro)
{	
	float32_t *X = ukf->X_f32;
	float32_t norm;

	//initialise quaternion state
	X[0] = q[0];
	X[1] = q[1];
	X[2] = q[2];
	X[3] = q[3];

	norm = FastInvSqrt(X[0] * X[0] + X[1] * X[1] + X[2] * X[2] + X[3] * X[3]);
	X[0] *= norm;
	X[1] *= norm;
	X[2] *= norm;
	X[3] *= norm;

	//initialise gyro state
	X[4] = gyro[0];
	X[5] = gyro[0];
	X[6] = gyro[0];
}

void CKF_Update(CKF_Filter* ukf, float32_t *q, float32_t *gyro, float32_t *accel, float32_t *mag, float32_t dt)
{
	int col, row;
	float32_t norm;
	float32_t halfdx, halfdy, halfdz;
	float32_t halfdt = 0.5f * dt;
	//////////////////////////////////////////////////////////////////////////
	float32_t q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;
	float32_t _2q0,_2q1,_2q2,_2q3;
	//
	float32_t hx, hy, hz;
	float32_t bx, bz;
	float32_t _2mx, _2my, _2mz;
	//
	float32_t *X = ukf->X_f32, *Y = ukf->Y_f32;
	float32_t *tmpX = ukf->tmpX_f32, *tmpY = ukf->tmpY_f32;
	float32_t *iKesi = ukf->iKesi_f32;
	float32_t *Xminus = ukf->Xminus_f32;
	float32_t *Yminus = ukf->Yminus_f32;
	float32_t tmpQ[4];
	//////////////////////////////////////////////////////////////////////////
	//time update
	//evaluate the cholesky factor
	arm_mat_chol_f32(&ukf->P);
	arm_mat_remainlower_f32(&ukf->P);

	//evaluate the cubature points
	arm_mat_getcolumn_f32(&ukf->Kesi, iKesi, 0);
	arm_mat_mult_f32(&ukf->P, &ukf->iKesi, &ukf->tmpX);
	arm_mat_add_f32(&ukf->tmpX, &ukf->X, &ukf->tmpX);
	arm_mat_setcolumn_f32(&ukf->XCP, tmpX, 0);
	//
	//evaluate the propagated cubature points
	halfdx = halfdt * tmpX[4];
	halfdy = halfdt * tmpX[5];
	halfdz = halfdt * tmpX[6];
	//
	tmpQ[0] = tmpX[0];
	tmpQ[1] = tmpX[1];
	tmpQ[2] = tmpX[2];
	tmpQ[3] = tmpX[3];
	//model prediction
	//simple way, pay attention!!!
	//according to the actual gyroscope output
	//and coordinate system definition
	tmpX[0] = tmpQ[0] + (halfdx * tmpQ[1] + halfdy * tmpQ[2] + halfdz * tmpQ[3]);
	tmpX[1] = tmpQ[1] - (halfdx * tmpQ[0] + halfdy * tmpQ[3] - halfdz * tmpQ[2]);
	tmpX[2] = tmpQ[2] + (halfdx * tmpQ[3] - halfdy * tmpQ[0] - halfdz * tmpQ[1]);
	tmpX[3] = tmpQ[3] - (halfdx * tmpQ[2] - halfdy * tmpQ[1] + halfdz * tmpQ[0]);
	//////////////////////////////////////////////////////////////////////////
	//re-normalize quaternion
	norm = FastInvSqrt(tmpX[0] * tmpX[0] + tmpX[1] * tmpX[1] + tmpX[2] * tmpX[2] + tmpX[3] * tmpX[3]);
	tmpX[0] *= norm;
	tmpX[1] *= norm;
	tmpX[2] *= norm;
	tmpX[3] *= norm;
	//
	arm_mat_setcolumn_f32(&ukf->XminusCP, tmpX, 0);
	for(row = 0; row < CKF_STATE_DIM; row++){
		X[row] = tmpX[row];
	}
	for(col = 1; col < CKF_CP_POINTS; col++){
		//evaluate the cubature points
		arm_mat_getcolumn_f32(&ukf->Kesi, iKesi, col);
		arm_mat_mult_f32(&ukf->P, &ukf->iKesi, &ukf->tmpX);
		arm_mat_add_f32(&ukf->tmpX, &ukf->X, &ukf->tmpX);
		arm_mat_setcolumn_f32(&ukf->XCP, tmpX, col);
		//
		//evaluate the propagated cubature points
		halfdx = halfdt * tmpX[4];
		halfdy = halfdt * tmpX[5];
		halfdz = halfdt * tmpX[6];
		//
		tmpQ[0] = tmpX[0];
		tmpQ[1] = tmpX[1];
		tmpQ[2] = tmpX[2];
		tmpQ[3] = tmpX[3];
		//model prediction
		//simple way, pay attention!!!
		//according to the actual gyroscope output
		//and coordinate system definition
		tmpX[0] = tmpQ[0] + (halfdx * tmpQ[1] + halfdy * tmpQ[2] + halfdz * tmpQ[3]);
		tmpX[1] = tmpQ[1] - (halfdx * tmpQ[0] + halfdy * tmpQ[3] - halfdz * tmpQ[2]);
		tmpX[2] = tmpQ[2] + (halfdx * tmpQ[3] - halfdy * tmpQ[0] - halfdz * tmpQ[1]);
		tmpX[3] = tmpQ[3] - (halfdx * tmpQ[2] - halfdy * tmpQ[1] + halfdz * tmpQ[0]);
		//////////////////////////////////////////////////////////////////////////
		//re-normalize quaternion
		norm = FastInvSqrt(tmpX[0] * tmpX[0] + tmpX[1] * tmpX[1] + tmpX[2] * tmpX[2] + tmpX[3] * tmpX[3]);
		tmpX[0] *= norm;
		tmpX[1] *= norm;
		tmpX[2] *= norm;
		tmpX[3] *= norm;
		//
		arm_mat_setcolumn_f32(&ukf->XminusCP, tmpX, col);
		for(row = 0; row < CKF_STATE_DIM; row++){
			X[row] += tmpX[row];
		}
	}
	//estimate the predicted state
	arm_mat_scale_f32(&ukf->X, ukf->W, &ukf->X);
	//estimate the predicted error covariance
	arm_mat_getcolumn_f32(&ukf->XminusCP, Xminus, 0);
	arm_mat_trans_f32(&ukf->Xminus, &ukf->XminusT);
	arm_mat_mult_f32(&ukf->Xminus, &ukf->XminusT, &ukf->P);
	for(col = 1; col < CKF_CP_POINTS; col++){
		arm_mat_getcolumn_f32(&ukf->XminusCP, Xminus, col);
		arm_mat_trans_f32(&ukf->Xminus, &ukf->XminusT);
		arm_mat_mult_f32(&ukf->Xminus, &ukf->XminusT, &ukf->PX);
		arm_mat_add_f32(&ukf->P, &ukf->PX, &ukf->P);
	}
	arm_mat_scale_f32(&ukf->P, ukf->W, &ukf->P);
	arm_mat_trans_f32(&ukf->X, &ukf->XT);
	arm_mat_mult_f32(&ukf->X, &ukf->XT, &ukf->PX);
	arm_mat_sub_f32(&ukf->P, &ukf->PX, &ukf->P);
	arm_mat_add_f32(&ukf->P, &ukf->Q, &ukf->P);
	
	//////////////////////////////////////////////////////////////////////////
	//measurement update
	//evaluate the cholesky factor
	arm_mat_chol_f32(&ukf->P);
	arm_mat_remainlower_f32(&ukf->P);
		
	//evaluate the cubature points
	arm_mat_getcolumn_f32(&ukf->Kesi, iKesi, 0);
	arm_mat_mult_f32(&ukf->P, &ukf->iKesi, &ukf->tmpX);
	arm_mat_add_f32(&ukf->tmpX, &ukf->X, &ukf->tmpX);
	arm_mat_setcolumn_f32(&ukf->XCP, tmpX, 0);
	
	//normalize accel and mag
	norm = FastInvSqrt(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);
	accel[0] *= norm;
	accel[1] *= norm;
	accel[2] *= norm;
	//////////////////////////////////////////////////////////////////////////
	norm = FastInvSqrt(mag[0] * mag[0] + mag[1] * mag[1] + mag[2] * mag[2]);
	mag[0] *= norm;
	mag[1] *= norm;
	mag[2] *= norm;

	_2mx = 2.0f * mag[0];
	_2my = 2.0f * mag[1];
	_2mz = 2.0f * mag[2];
	
	//auxiliary variables to avoid repeated arithmetic
	_2q0 = 2.0f * tmpX[0];
	_2q1 = 2.0f * tmpX[1];
	_2q2 = 2.0f * tmpX[2];
	_2q3 = 2.0f * tmpX[3];
	//
	q0q0 = tmpX[0] * tmpX[0];
	q0q1 = tmpX[0] * tmpX[1];
	q0q2 = tmpX[0] * tmpX[2];
	q0q3 = tmpX[0] * tmpX[3];
	q1q1 = tmpX[1] * tmpX[1];
	q1q2 = tmpX[1] * tmpX[2];
	q1q3 = tmpX[1] * tmpX[3];
	q2q2 = tmpX[2] * tmpX[2];
	q2q3 = tmpX[2] * tmpX[3];
	q3q3 = tmpX[3] * tmpX[3];

	//reference direction of earth's magnetic field
	hx = _2mx * (0.5f - q2q2 - q3q3) + _2my * (q1q2 - q0q3) + _2mz *(q1q3 + q0q2);
	hy = _2mx * (q1q2 + q0q3) + _2my * (0.5f - q1q1 - q3q3) + _2mz * (q2q3 - q0q1);
	hz = _2mx * (q1q3 - q0q2) + _2my * (q2q3 + q0q1) + _2mz *(0.5f - q1q1 - q2q2);
	arm_sqrt_f32(hx * hx + hy * hy, &bx);
	bz = hz;

	tmpY[0] = tmpX[0];
	tmpY[1] = tmpX[1];
	tmpY[2] = tmpX[2];
	tmpY[3] = tmpX[3];
	tmpY[4] = 2.0f * (q1q3 - q0q2);
	tmpY[5] = 2.0f * (q2q3 + q0q1);
	tmpY[6] = -1.0f + 2.0f * (q0q0 + q3q3);
	tmpY[7] = tmpX[4];
	tmpY[8] = tmpX[5];
	tmpY[9] = tmpX[6];
	tmpY[10] = bx * (1.0f - 2.0f * (q2q2 + q3q3)) + bz * ( 2.0f * (q1q3 - q0q2));
	tmpY[11] = bx * (2.0f * (q1q2 - q0q3)) + bz * (2.0f * (q2q3 + q0q1));
	tmpY[12] = bx * (2.0f * (q1q3 + q0q2)) + bz * (1.0f - 2.0f * (q1q1 + q2q2));
	
	arm_mat_setcolumn_f32(&ukf->YCP, tmpY, 0);
	for(row = 0; row < CKF_MEASUREMENT_DIM; row++){
		Y[row] = tmpY[row];
	}
	
	for(col = 1; col < CKF_CP_POINTS; col++){
		//evaluate the cubature points
		arm_mat_getcolumn_f32(&ukf->Kesi, iKesi, col);
		arm_mat_mult_f32(&ukf->P, &ukf->iKesi, &ukf->tmpX);
		arm_mat_add_f32(&ukf->tmpX, &ukf->X, &ukf->tmpX);
		arm_mat_setcolumn_f32(&ukf->XCP, tmpX, col);
		
		//auxiliary variables to avoid repeated arithmetic
		_2q0 = 2.0f * tmpX[0];
		_2q1 = 2.0f * tmpX[1];
		_2q2 = 2.0f * tmpX[2];
		_2q3 = 2.0f * tmpX[3];
		//
		q0q0 = tmpX[0] * tmpX[0];
		q0q1 = tmpX[0] * tmpX[1];
		q0q2 = tmpX[0] * tmpX[2];
		q0q3 = tmpX[0] * tmpX[3];
		q1q1 = tmpX[1] * tmpX[1];
		q1q2 = tmpX[1] * tmpX[2];
		q1q3 = tmpX[1] * tmpX[3];
		q2q2 = tmpX[2] * tmpX[2];
		q2q3 = tmpX[2] * tmpX[3];
		q3q3 = tmpX[3] * tmpX[3];

		//reference direction of earth's magnetic field
		hx = _2mx * (0.5f - q2q2 - q3q3) + _2my * (q1q2 - q0q3) + _2mz *(q1q3 + q0q2);
		hy = _2mx * (q1q2 + q0q3) + _2my * (0.5f - q1q1 - q3q3) + _2mz * (q2q3 - q0q1);
		hz = _2mx * (q1q3 - q0q2) + _2my * (q2q3 + q0q1) + _2mz *(0.5f - q1q1 - q2q2);
		arm_sqrt_f32(hx * hx + hy * hy, &bx);
		bz = hz;

		tmpY[0] = tmpX[0];
		tmpY[1] = tmpX[1];
		tmpY[2] = tmpX[2];
		tmpY[3] = tmpX[3];
		tmpY[4] = 2.0f * (q1q3 - q0q2);
		tmpY[5] = 2.0f * (q2q3 + q0q1);
		tmpY[6] = -1.0f + 2.0f * (q0q0 + q3q3);
		tmpY[7] = tmpX[4];
		tmpY[8] = tmpX[5];
		tmpY[9] = tmpX[6];
		tmpY[10] = bx * (1.0f - 2.0f * (q2q2 + q3q3)) + bz * ( 2.0f * (q1q3 - q0q2));
		tmpY[11] = bx * (2.0f * (q1q2 - q0q3)) + bz * (2.0f * (q2q3 + q0q1));
		tmpY[12] = bx * (2.0f * (q1q3 + q0q2)) + bz * (1.0f - 2.0f * (q1q1 + q2q2));
		arm_mat_setcolumn_f32(&ukf->YCP, tmpY, col);
		for(row = 0; row < CKF_MEASUREMENT_DIM; row++){
			Y[row] += tmpY[row];
		}
	}
	//estimate the predicted measurement
	arm_mat_scale_f32(&ukf->Y, ukf->W, &ukf->Y);
	//estimate the innovation covariance matrix
	arm_mat_getcolumn_f32(&ukf->YCP, Yminus, 0);
	arm_mat_trans_f32(&ukf->Yminus, &ukf->YminusT);
	arm_mat_mult_f32(&ukf->Yminus, &ukf->YminusT, &ukf->PY);
	for(col = 1; col < CKF_CP_POINTS; col++){
		arm_mat_getcolumn_f32(&ukf->YCP, Yminus, col);
		arm_mat_trans_f32(&ukf->Yminus, &ukf->YminusT);
		arm_mat_mult_f32(&ukf->Yminus, &ukf->YminusT, &ukf->tmpPY);
		arm_mat_add_f32(&ukf->PY, &ukf->tmpPY, &ukf->PY);
	}
	arm_mat_scale_f32(&ukf->PY, ukf->W, &ukf->PY);
	arm_mat_trans_f32(&ukf->Y, &ukf->YT);
	arm_mat_mult_f32(&ukf->Y, &ukf->YT, &ukf->tmpPY);
	arm_mat_sub_f32(&ukf->PY, &ukf->tmpPY, &ukf->PY);
	arm_mat_add_f32(&ukf->PY, &ukf->R, &ukf->PY);
	
	//estimate the cross-covariance matrix
	arm_mat_getcolumn_f32(&ukf->XCP, Xminus, 0);
	arm_mat_getcolumn_f32(&ukf->YCP, Yminus, 0);
	arm_mat_trans_f32(&ukf->Yminus, &ukf->YminusT);
	arm_mat_mult_f32(&ukf->Xminus, &ukf->YminusT, &ukf->PXY);
	arm_mat_scale_f32(&ukf->PXY, ukf->W, &ukf->PXY);
	for(col = 1; col < CKF_CP_POINTS; col++){
		arm_mat_getcolumn_f32(&ukf->XCP, Xminus, col);
		arm_mat_getcolumn_f32(&ukf->YCP, Yminus, col);
		arm_mat_trans_f32(&ukf->Yminus, &ukf->YminusT);
		arm_mat_mult_f32(&ukf->Xminus, &ukf->YminusT, &ukf->tmpPXY);
		arm_mat_scale_f32(&ukf->tmpPXY, ukf->W, &ukf->tmpPXY);
		arm_mat_add_f32(&ukf->PXY, &ukf->tmpPXY, &ukf->PXY);
	}
	//arm_mat_scale_f32(&ukf->PXY, ukf->W, &ukf->PXY);
	arm_mat_trans_f32(&ukf->Y, &ukf->YT);
	arm_mat_mult_f32(&ukf->X, &ukf->YT, &ukf->tmpPXY);
	arm_mat_sub_f32(&ukf->PXY, &ukf->tmpPXY, &ukf->PXY);
	//estimate the kalman gain
	//K = PXY * inv(PY);
	arm_mat_inverse_f32(&ukf->PY, &ukf->tmpPY);
	arm_mat_mult_f32(&ukf->PXY, &ukf->tmpPY, &ukf->K);
	//estimate the updated state
	//X = X + K*(z - Y);
	Y[0] = q[0] - Y[0];
	Y[1] = q[1] - Y[1];
	Y[2] = q[2] - Y[2];
	Y[3] = q[3] - Y[3];
	//
	Y[4] = accel[0] - Y[4];
	Y[5] = accel[1] - Y[5];
	Y[6] = accel[2] - Y[6];
	Y[7] = gyro[0] - Y[7];
	Y[8] = gyro[1] - Y[8];
	Y[9] = gyro[2] - Y[9];
	//////////////////////////////////////////////////////////////////////////
	Y[10] = mag[0] - Y[10];
	Y[11] = mag[1] - Y[11];
	Y[12] = mag[2] - Y[12];

	arm_mat_mult_f32(&ukf->K, &ukf->Y, &ukf->tmpX);
	arm_mat_add_f32(&ukf->X, &ukf->tmpX, &ukf->X);
	//normalize quaternion
	norm = FastInvSqrt(X[0] * X[0] + X[1] * X[1] + X[2] * X[2] + X[3] * X[3]);
	X[0] *= norm;
	X[1] *= norm;
	X[2] *= norm;
	X[3] *= norm;
	//estimate the corresponding error covariance
	//the default tuning parameters don't work properly
	//must tune the P,Q,R and calibrate the sensor data
	//P = PX - K * PY * K'
	arm_mat_trans_f32(&ukf->K, &ukf->KT);
	arm_mat_mult_f32(&ukf->K, &ukf->PY, &ukf->PXY);
	arm_mat_mult_f32(&ukf->PXY, &ukf->KT, &ukf->P);
	arm_mat_sub_f32(&ukf->PX, &ukf->P, &ukf->P);
}

void CKF_GetAngle(CKF_Filter* ukf, float32_t* rpy)
{
	float32_t R[3][3];
	float32_t *X = ukf->X_f32;
	//Z-Y-X
	R[0][0] = 2.0f * (X[0] * X[0] + X[1] * X[1]) - 1.0f;
	R[0][1] = 2.0f * (X[1] * X[2] + X[0] * X[3]);
	R[0][2] = 2.0f * (X[1] * X[3] - X[0] * X[2]);
	//R[1][0] = 2.0f * (X[1] * X[2] - X[0] * X[3]);
	//R[1][1] = 2.0f * (X[0] * X[0] + X[2] * X[2]) - 1.0f;
	R[1][2] = 2.0f * (X[2] * X[3] + X[0] * X[1]);
	//R[2][0] = 2.0f * (X[1] * X[3] + X[0] * X[2]);
	//R[2][1] = 2.0f * (X[2] * X[3] - X[0] * X[1]);
	R[2][2] = 2.0f * (X[0] * X[0] + X[3] * X[3]) - 1.0f;

	//roll
	rpy[0] = FastAtan2(R[1][2], R[2][2]);
	if (rpy[0] == CKF_PI)
		rpy[0] = -CKF_PI;
	//pitch
	if (R[0][2] >= 1.0f)
		rpy[1] = -CKF_HALFPI;
	else if (R[0][2] <= -1.0f)
		rpy[1] = CKF_HALFPI;
	else
		rpy[1] = FastAsin(-R[0][2]);
	//yaw
	rpy[2] = FastAtan2(R[0][1], R[0][0]);
	if (rpy[2] < 0.0f){
		rpy[2] += CKF_TWOPI;
	}
	if (rpy[2] > CKF_TWOPI){
		rpy[2] = 0.0f;
	}

	rpy[0] = CKF_TODEG(rpy[0]);
	rpy[1] = CKF_TODEG(rpy[1]);
	rpy[2] = CKF_TODEG(rpy[2]);
}